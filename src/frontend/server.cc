#include <chrono>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "net/socket.hh"
#include "util/eventloop.hh"
#include "util/exception.hh"
#include "util/timerfd.hh"

using namespace std;

void usage( const char* argv0 )
{
  cerr << "Usage: " << argv0 << " IP PORT" << endl;
}

struct Connection
{
  Connection( TCPSocket&& s )
    : socket( move( s ) )
  {
  }

  TCPSocket socket;
  string buffer {};
};

int main( int argc, char* argv[] )
{
  if ( argc <= 0 ) {
    abort();
  }

  if ( argc != 3 ) {
    usage( argv[0] );
    return EXIT_FAILURE;
  }

  try {
    const string ip { argv[1] };
    const uint16_t port { static_cast<uint16_t>( stoul( argv[2] ) ) };

    string READ_BUFFER;
    READ_BUFFER.resize( 1024 * 1024 );

    Address listen_address { ip, port };
    TCPSocket listener_socket;
    listener_socket.set_blocking( false );
    listener_socket.set_reuseaddr();
    listener_socket.bind( listen_address );
    listener_socket.listen();

    list<TCPSocket> new_sockets;
    list<Connection> connections;

    EventLoop event_loop;
    TimerFD output_timer { chrono::seconds { 1 } };

    const auto el_listen_cateogry = event_loop.add_category( "Listener" );
    const auto el_client_category = event_loop.add_category( "Client" );

    size_t total_sum = 0;

    event_loop.add_rule(
      el_listen_cateogry,
      Direction::In,
      listener_socket,
      [&] { new_sockets.emplace_back( move( listener_socket.accept() ) ); },
      [] { return true; } );

    event_loop.add_rule(
      "Output Timer",
      Direction::In,
      output_timer,
      [&total_sum, &output_timer] {
        output_timer.read_event();
        cerr << "total sum = " << total_sum << endl;
      },
      [] { return true; } );

    // event_loop.set_fd_failure_callback( [] {} );

    while ( event_loop.wait_next_event( -1 ) != EventLoop::Result::Exit ) {
      while ( not new_sockets.empty() ) {
        connections.emplace_back( move( new_sockets.front() ) );
        new_sockets.pop_front();

        auto conn_it = prev( connections.end() );

        event_loop.add_rule(
          el_client_category,
          conn_it->socket,
          [conn_it, &connections, &total_sum, &READ_BUFFER] {
            const auto len = conn_it->socket.read( { READ_BUFFER } );

            if ( not len ) {
              return;
            }

            conn_it->buffer += READ_BUFFER.substr( 0, len );
            const auto end_of_input = conn_it->buffer.find_first_of( "\r\n" );

            if ( end_of_input != string::npos ) {
              total_sum += stoi( conn_it->buffer.substr( 0, end_of_input ) );
              conn_it->socket.shutdown( SHUT_RDWR );
            }
          },
          [] { return true; },
          [] {},
          [] { return false; },
          [conn_it, &connections] { connections.erase( conn_it ); } );
      }
    }

  } catch ( exception& ex ) {
    print_exception( argv[0], ex );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
