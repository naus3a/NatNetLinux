/*
 * FrameCtrlExample.cpp is Copyright 2016,
 * Enrico Viola <naus3a@gmail.com>
 *
 * NatNetLinux is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * NatNetLinux is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NatNetLinux.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <NatNetLinux/NatNet.h>
#include <NatNetLinux/CommandListener.h>
#include <NatNetLinux/FrameGetter.h>

#include <boost/program_options.hpp>
#include <time.h>

class Globals
{
public:
   
   // Parameters read from the command line
   static uint32_t localAddress;
   static uint32_t serverAddress;
   
   // State of the main() thread.
   static bool run;
};
uint32_t Globals::localAddress = 0;
uint32_t Globals::serverAddress = 0;
bool Globals::run = false;

// End the program gracefully.
void terminate(int)
{
   // Tell the main() thread to close.
   Globals::run = false;
}

// Set the global addresses from the command line.
void readOpts( int argc, char* argv[] )
{
   namespace po = boost::program_options;
   
   po::options_description desc("frameCtrlExample: same as Simple-Example, but with direct control over frames");
   desc.add_options()
      ("help", "Display help message")
      ("local-addr,l", po::value<std::string>(), "Local IPv4 address")
      ("server-addr,s", po::value<std::string>(), "Server IPv4 address")
   ;
   
   po::variables_map vm;
   po::store(po::parse_command_line(argc,argv,desc), vm);
   
   if(
      argc < 5 || vm.count("help") ||
      !vm.count("local-addr") ||
      !vm.count("server-addr")
   )
   {
      std::cout << desc << std::endl;
      exit(1);
   }
   
   Globals::localAddress = inet_addr( vm["local-addr"].as<std::string>().c_str() );
   Globals::serverAddress = inet_addr( vm["server-addr"].as<std::string>().c_str() );
}

void printFrames(FrameGetter & frameGetter){
    Globals::run = true;
    while (Globals::run) {
        FrameGetter::FrameResult fr = frameGetter.nextFrame();
        if(fr==FrameGetter::SUCCESS){
            std::cout<<frameGetter.getLastFrame()<<std::endl;
        }
    }
}

// This thread loop just prints frames as they arrive.
/*void printFrames(FrameListener& frameListener)
{
   bool valid;
   MocapFrame frame;
   Globals::run = true;
   while(Globals::run)
   {
      while( true )
      {
         // Try to get a new frame from the listener.
         MocapFrame frame(frameListener.pop(&valid).first);
         // Quit if the listener has no more frames.
         if( !valid )
            break;
         std::cout << frame << std::endl;
      }
      
      // Sleep for a little while to simulate work :)
      usleep(1000);
   }
}*/


int main(int argc, char* argv[])
{
   // Version number of the NatNet protocol, as reported by the server.
   unsigned char natNetMajor;
   unsigned char natNetMinor;
   
   // Sockets
   int sdCommand;
   int sdData;
   
   // Catch ctrl-c and terminate gracefully.
   signal(SIGINT, terminate);
   
   // Set addresses
   readOpts( argc, argv );
   // Use this socket address to send commands to the server.
   struct sockaddr_in serverCommands = NatNet::createAddress(Globals::serverAddress, NatNet::commandPort);
   
   // Create sockets
   sdCommand = NatNet::createCommandSocket( Globals::localAddress );
   sdData = NatNet::createDataSocket( Globals::localAddress );
   
   // Start the CommandListener in a new thread.
   CommandListener commandListener(sdCommand);
   commandListener.start();

   // Send a ping packet to the server so that it sends us the NatNet version
   // in its response to commandListener.
   NatNetPacket ping = NatNetPacket::pingPacket();
   ping.send(sdCommand, serverCommands);
   
   // Wait here for ping response to give us the NatNet version.
   commandListener.getNatNetVersion(natNetMajor, natNetMinor);
   std::cout<<"Server version: "<<natNetMajor<<"."<<natNetMinor<<std::endl;

    FrameGetter frameGetter(sdData, natNetMajor, natNetMinor);
    printFrames(frameGetter);
   
   // Start up a FrameListener in a new thread.
   //FrameListener frameListener(sdData, natNetMajor, natNetMinor);
   //frameListener.start();
   
   // This infinite loop simulates a "worker" thread that reads the frame
   // buffer each time through, and exits when ctrl-c is pressed.
   //printFrames(frameListener);
   //timeStats(frameListener);
   
   // Wait for threads to finish.
   //frameListener.stop();
   commandListener.stop();
   //frameListener.join();
   commandListener.join();
   
   // Epilogue
   close(sdData);
   close(sdCommand);
   return 0;
}
