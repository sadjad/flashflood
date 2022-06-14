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
  cerr << "Usage: " << argv0 << " SERVER_IP SERVER_PORT COUNT" << endl;
}

struct Connection
{
  TCPSocket socket {};
  string buffer {};
};

int main( int argc, char* argv[] )
{
  if ( argc <= 0 ) {
    abort();
  }

  if ( argc != 4 ) {
    usage( argv[0] );
    return EXIT_FAILURE;
  }

  try {
    string server_ip { argv[1] };
    const uint16_t server_port { static_cast<uint16_t>( stoul( argv[2] ) ) };
    const size_t connection_count { stoull( argv[3] ) };

    Address server_address { server_ip, server_port };

    EventLoop event_loop;
    const auto event_loop_client_category = event_loop.add_category( "Client" );

    list<Connection> connections;

    for ( size_t i = 0; i < connection_count; i++ ) {
      connections.emplace_back();
      auto conn_it = prev( connections.end() );

      conn_it->socket.set_blocking( false );
      conn_it->socket.connect( server_address );
      conn_it->buffer = "1\n";

      event_loop.add_rule(
        event_loop_client_category,
        Direction::Out,
        conn_it->socket,
        [conn_it] {
          const auto len = conn_it->socket.write( conn_it->buffer );
          conn_it->buffer.erase( 0, len );
        },
        [conn_it] { return not conn_it->buffer.empty(); },
        [conn_it, &connections] { connections.erase( conn_it ); } );
    }

    while ( event_loop.wait_next_event( -1 ) != EventLoop::Result::Exit ) {
    }
  } catch ( exception& ex ) {
    print_exception( argv[0], ex );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
