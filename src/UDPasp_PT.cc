/******************************************************************************
* Copyright (c) 2004, 2014  Ericsson AB
* All rights reserved. This program and the accompanying materials
* are made available under the terms of the Eclipse Public License v2.0
* which accompanies this distribution, and is available at
* https://www.eclipse.org/org/documents/epl-2.0/EPL-2.0.html
*
* Contributors:
*   Jozsef Gyurusi - initial implementation and initial documentation
*   Csaba Bela Koppany
*   Gabor Szalai
*   Peter Kremer
*   Tamas Buti
*   Zoltan Jasz
******************************************************************************/
//
//  File:               UDPasp_PT.cc
//  Description:        UDP test port source
//  Rev:                R8A
//  Prodnr:             CNL 113 346
//


#include "UDPasp_PT.hh"

#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <memory.h>

#define DEFAULT_LOCAL_PORT	(50000)
#define DEFAULT_NUM_CONN	(10)

namespace UDPasp__PortType {

UDPasp__PT_PROVIDER::UDPasp__PT_PROVIDER(const char *par_port_name)
	: PORT(par_port_name)
	, debugging(false)
	, target_fd(-1)
{
  localAddr.sin_family = AF_INET;
  localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  localAddr.sin_port = htons(DEFAULT_LOCAL_PORT);
  port_mode = false;
  broadcast = false;
  conn_list = NULL;
  num_of_conn = 0;
  conn_list_length = 0;
  target_fd = -1;
}

UDPasp__PT_PROVIDER::~UDPasp__PT_PROVIDER()
{
  Free(conn_list);
}

void UDPasp__PT_PROVIDER::setUpSocket()
{
  log("entering UDPasp__PT::setUpSocket()");

  /* socket creation */
  if((target_fd = socket(AF_INET,SOCK_DGRAM,0))<0) {
    TTCN_error("Cannot open socket \n");
  }

  if(broadcast){
    int on=1;
    if( setsockopt( target_fd, SOL_SOCKET, SO_BROADCAST, (char *)&on, sizeof(on) ) < 0 )
    {
      TTCN_error("Setsockopt error: SO_BROADCAST");
    }
  }
  
  log("Binding port...");
  if(bind(target_fd, (struct sockaddr *) &localAddr, sizeof(localAddr))<0) {
    TTCN_error("Cannot bind port\n");
  }

  log("leaving UDPasp__PT::setUpSocket()");
}

void UDPasp__PT_PROVIDER::closeDownSocket()
{
  log("entering UDPasp__PT::closeDownSocket()");
  close(target_fd);
  target_fd = -1;
  log("entering UDPasp__PT::closeDownSocket()");
}

void UDPasp__PT_PROVIDER::log(const char *fmt, ...)
{
    if (debugging) {
		TTCN_Logger::begin_event(TTCN_DEBUG);
		TTCN_Logger::log_event("UDP test port (%s): ", get_name());
		va_list args;
		va_start(args, fmt);
		TTCN_Logger::log_event_va_list(fmt, args);
		va_end(args);
		TTCN_Logger::end_event();
    }
}

void UDPasp__PT_PROVIDER::logHex(const char *prompt, const OCTETSTRING& msg)
{
    if (debugging) { //if debug
      TTCN_Logger::begin_event(TTCN_DEBUG);
      TTCN_Logger::log_event_str(prompt);
      TTCN_Logger::log_event("Size: %d,\nMsg: ",msg.lengthof());

      for(int i=0; i<msg.lengthof(); i++)
	  	TTCN_Logger::log_event(" %02x", ((const unsigned char*)msg)[i]);
      TTCN_Logger::log_event("\n");
      TTCN_Logger::end_event();
    }
}

unsigned long UDPasp__PT_PROVIDER::getHostId(const char* hostName)
{
    log("UDPasp__PT::getHostId called");
    unsigned long ipAddress = 0;

    if(strcmp(hostName, "255.255.255.255") == 0) {
      ipAddress = 0xffffffff;
    } else {
      in_addr_t addr = inet_addr(hostName);
      if (addr != (in_addr_t) - 1) {     // host name in XX:XX:XX:XX form
        ipAddress = addr;
      }
      else {                               // host name in domain.com form
        struct hostent* hptr;
        if ((hptr = gethostbyname(hostName)) == 0)
          TTCN_error("The host name %s is not valid.", hostName);
          ipAddress = *((unsigned long*)hptr->h_addr_list[0]);
      }
    }

    log("Host name: %s, Host address: %u", (const char*)hostName, ipAddress);
    log("UDPasp__PT::getHostId exited");

    return htonl ( ipAddress );
}

void UDPasp__PT_PROVIDER::set_parameter(const char *parameter_name,
	const char *parameter_value)
{
	log("entering UDPasp__PT::set_parameter(%s, %s)", parameter_name, parameter_value);
	if (!strcmp(parameter_name, "debugging")) {
		if (!strcmp(parameter_value,"YES") || !strcmp(parameter_value,"yes"))
			debugging = true;
	}else if(!strcmp(parameter_name,"localIPAddr")){
		localAddr.sin_addr.s_addr = htonl(getHostId(parameter_value));
	}else if(!strcmp(parameter_name,"broadcast")){
		if (!strcasecmp(parameter_value,"enabled"))
			broadcast = true;
    else if (!strcasecmp(parameter_value,"disabled"))
			broadcast = false;
    else {
      broadcast = false;
      	TTCN_warning("UDPasp__PT::set_parameter(): Unsupported Test Port parameter value: %s", parameter_value);
    }
	}else if(!strcmp(parameter_name,"localPort")){
		localAddr.sin_port = htons(atoi(parameter_value));
	}else if (!strcmp(parameter_name, "mode")) {
		if (!strcasecmp(parameter_value,"advanced"))
			port_mode = true;
	}else
		TTCN_warning("UDPasp__PT::set_parameter(): Unsupported Test Port parameter: %s", parameter_name);

	log("leaving UDPasp__PT::set_parameter(%s, %s)", parameter_name, parameter_value);
}

void UDPasp__PT_PROVIDER::Event_Handler(const fd_set *read_fds,
	const fd_set */*write_fds*/, const fd_set */*error_fds*/,
	double /*time_since_last_call*/)
{
  log("entering UDPasp__PT::Event_Handler()");
  unsigned char msg[65535];        // Allocate memory for possible messages
  int msgLength;
  struct sockaddr_in remoteAddr;
  socklen_t addr_length = sizeof(remoteAddr);

  if(port_mode){
    int conn_found=0;
    for(int a=0;a<conn_list_length;a++){
      if(conn_list[a].status){
        conn_found++;
        if(FD_ISSET(conn_list[a].fd,read_fds)){
          if ((msgLength = recvfrom(conn_list[a].fd, (char*)msg, sizeof(msg), 0,
                 (struct sockaddr*)&remoteAddr, &addr_length)) < 0)
               	        TTCN_error("Error when reading the received UDP PDU.");
          logHex("Message received:  ", OCTETSTRING(msgLength, msg));
          log("The remote port:          %d", remoteAddr.sin_port);
          char *remote_address = inet_ntoa(remoteAddr.sin_addr);
          log("The remote address:       %s", remote_address);

          UDPasp__Types::ASP__UDP__message parameters;
          parameters.data() = OCTETSTRING(msgLength, msg);
          parameters.remote__addr() = CHARSTRING(remote_address);
          parameters.remote__port() = ntohs(remoteAddr.sin_port);
          parameters.id()=a;

          logHex("Recevied PDU   ", parameters.data() );

          incoming_message(parameters);
        }
      }
      if(conn_found==num_of_conn) {break;}
    }
  }
  else{
    if ((msgLength = recvfrom(target_fd, (char*)msg, sizeof(msg), 0, (struct sockaddr*)&remoteAddr, &addr_length)) < 0)
	  TTCN_error("Error when reading the received UDP PDU.");

    logHex("Message received:  ", OCTETSTRING(msgLength, msg));
    log("The remote port:          %d", remoteAddr.sin_port);
    char *remote_address = inet_ntoa(remoteAddr.sin_addr);
    log("The remote address:       %s", remote_address);

    UDPasp__Types::ASP__UDP parameters;
    parameters.data() = OCTETSTRING(msgLength, msg);
    parameters.addressf() = CHARSTRING(remote_address);
    parameters.portf() = ntohs(remoteAddr.sin_port);

    logHex("Recevied PDU   ", parameters.data() );

    incoming_message(parameters);
  }
  log("leaving UDPasp__PT::Event_Handler()");
}

void UDPasp__PT_PROVIDER::user_map(const char */*system_port*/)
{
  log("entering UDPasp__PT::user_map()");

  if(port_mode){
    if (conn_list != NULL) TTCN_error("UDP Test Port (%s): Internal error: "
      "conn_list is not NULL when mapping.", port_name);
    conn_list = (conn_data*)Malloc(DEFAULT_NUM_CONN * sizeof(*conn_list));
    num_of_conn=0;
    conn_list_length=DEFAULT_NUM_CONN;
    for(int a=0;a<conn_list_length;a++){conn_list[a].status=0;}
    FD_ZERO(&conn_map);
  }
  else{
    setUpSocket();
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(target_fd, &readfds);
    Install_Handler(&readfds, NULL, NULL, 0.0);
  }
  log("leaving UDPasp__PT::user_map()");
}

void UDPasp__PT_PROVIDER::user_unmap(const char */*system_port*/)
{
  log("entering UDPasp__PT::user_unmap()");
  if(port_mode){
    for(int a=0;a<conn_list_length;a++){
      if(conn_list[a].status==1){
        close(conn_list[a].fd);
      }
    }
    Free(conn_list);
    conn_list = NULL;
  }
  else{
    closeDownSocket();
  }
  Uninstall_Handler();
  log("leaving UDPasp__PT::user_unmap()");
}

void UDPasp__PT_PROVIDER::user_start()
{
}

void UDPasp__PT_PROVIDER::user_stop()
{
}

void UDPasp__PT_PROVIDER::outgoing_send(const UDPasp__Types::ASP__UDP& send_par)
{
  log("entering UDPasp__PT::outgoing_send(ASP__UDP)");
  logHex("Sending data: ", send_par.data());

  struct sockaddr_in remoteAddr;
  remoteAddr.sin_family = AF_INET;
  remoteAddr.sin_addr.s_addr = htonl(getHostId((const char*)send_par.addressf()));
  remoteAddr.sin_port = htons(send_par.portf());

  int nrOfBytesSent = sendto(target_fd, (const char*)(const unsigned char*)send_par.data(),
		send_par.data().lengthof(), 0, (struct sockaddr*)&remoteAddr, sizeof(remoteAddr));

  log("Nr of bytes sent = %d", nrOfBytesSent);

  if (nrOfBytesSent != send_par.data().lengthof()) {
		TTCN_error("Sendto system call failed: %d bytes was sent instead of %d", nrOfBytesSent, send_par.data().lengthof());
	}

  log("leaving UDPasp__PT::outgoing_send(ASP__UDP)");
}

void UDPasp__PT_PROVIDER::outgoing_send(const UDPasp__Types::ASP__UDP__message& send_par)
{
  log("entering UDPasp__PT::outgoing_send(ASP__UDP__message)");
  int sock;
  struct sockaddr_in targetAddr;
  targetAddr.sin_family = AF_INET;

  if(send_par.id().ispresent()){
    int cn=send_par.id()();
    if(send_par.remote__addr().ispresent()){
      conn_list[cn].remote_Addr.sin_addr.s_addr=
             htonl(getHostId((const char*)send_par.remote__addr()()));
    }
    if(send_par.remote__port().ispresent()){
      conn_list[cn].remote_Addr.sin_port=htons(send_par.remote__port()());
    }
    targetAddr.sin_addr.s_addr=conn_list[cn].remote_Addr.sin_addr.s_addr;
    targetAddr.sin_port=conn_list[cn].remote_Addr.sin_port;
    if(targetAddr.sin_addr.s_addr==htonl(INADDR_ANY))
      TTCN_error("UDP: No remote host name specified.");
    if(targetAddr.sin_port==0)
      TTCN_error("UDP: No remote host port specified.");
    sock=conn_list[cn].fd;
  }
  else{
    targetAddr.sin_addr.s_addr = htonl(getHostId((const char*)send_par.remote__addr()()));
    targetAddr.sin_port = htons(send_par.remote__port()());
    if((sock = socket(AF_INET,SOCK_DGRAM,0))<0) {
      TTCN_error("Cannot open socket \n");
    }
    if(broadcast){
      int on=1;
      if( setsockopt( sock, SOL_SOCKET, SO_BROADCAST, (char *)&on, sizeof(on) ) < 0 )
      {
        TTCN_error("UDP test port: Setsockopt error: SO_BROADCAST");
      }
    }
  }

  int nrOfBytesSent = sendto(sock, (const char*)(const unsigned char*)send_par.data(),
		send_par.data().lengthof(), 0, (struct sockaddr*)&targetAddr, sizeof(targetAddr));

  log("Nr of bytes sent = %d", nrOfBytesSent);

  if (nrOfBytesSent != send_par.data().lengthof()) {
		TTCN_error("Sendto system call failed: %d bytes was sent instead of %d", nrOfBytesSent, send_par.data().lengthof());
	}
  if(!send_par.id().ispresent()) close(sock);

  log("leaving UDPasp__PT::outgoing_send(ASP__UDP__message)");
}

void UDPasp__PT_PROVIDER::outgoing_send(const UDPasp__Types::ASP__UDP__open& send_par)
{
  log("entering UDPasp__PT::outgoing_send(ASP__UDP__open)");
  int cn;
  int sock;
  socklen_t namelen;
  struct sockaddr_in localAddr;
  localAddr.sin_family = AF_INET;
  if(num_of_conn<conn_list_length){
    cn=0;
    while(conn_list[cn].status){cn++;}
  }
  else{
    conn_list = (conn_data*)Realloc(conn_list, 2 * conn_list_length * sizeof(*conn_list));
    for(int a=conn_list_length;a<conn_list_length*2;a++){conn_list[a].status=0;}
    cn=conn_list_length;
    conn_list_length*=2;
  }

  if((sock = socket(AF_INET,SOCK_DGRAM,0))<0) {
    TTCN_error("Cannot open socket \n");
  }
  if(broadcast){
    int on=1;
    if( setsockopt( sock, SOL_SOCKET, SO_BROADCAST, (char *)&on, sizeof(on) ) < 0 )
    {
      TTCN_error("UDP test port: Setsockopt error: SO_BROADCAST");
    }
  }

  if(send_par.local__addr().ispresent()){
    localAddr.sin_addr.s_addr=
           htonl(getHostId((const char*)send_par.local__addr()()));
  }
  else{
    localAddr.sin_addr.s_addr=htonl(INADDR_ANY);
  }

  if(send_par.local__port().ispresent()){
    localAddr.sin_port=htons(send_par.local__port()());
  }
  else{
    localAddr.sin_port=0;
  }

  if(bind(sock, (struct sockaddr *) &localAddr, sizeof(localAddr))<0) {
    TTCN_error("Cannot bind port\n");
  }

  namelen=sizeof(localAddr);
  if(getsockname(sock, (struct sockaddr *) &localAddr, &namelen)<0) {
    TTCN_error("getsockname failed\n");
  }

  conn_list[cn].fd=sock;
  conn_list[cn].port_num=ntohs(localAddr.sin_port);
  conn_list[cn].status=1;
  conn_list[cn].remote_Addr.sin_family = AF_INET;
  if(send_par.remote__addr().ispresent()){
    conn_list[cn].remote_Addr.sin_addr.s_addr=
           htonl(getHostId((const char*)send_par.remote__addr()()));
  }
  else{
    conn_list[cn].remote_Addr.sin_addr.s_addr=htonl(INADDR_ANY);
  }

  if(send_par.remote__port().ispresent()){
    conn_list[cn].remote_Addr.sin_port=htons(send_par.remote__port()());
  }
  else{
    conn_list[cn].remote_Addr.sin_port=0;
  }
  num_of_conn++;
  FD_SET(sock,&conn_map);
  Uninstall_Handler();
  Install_Handler(&conn_map, NULL, NULL, 0.0);
  UDPasp__Types::ASP__UDP__open__result result;
  result.id()=cn;
  result.local__addr()=inet_ntoa(localAddr.sin_addr);
  result.local__port()=conn_list[cn].port_num;
  incoming_message(result);
  log("leaving UDPasp__PT::outgoing_send(ASP__UDP__open)");
}

void UDPasp__PT_PROVIDER::outgoing_send(const UDPasp__Types::ASP__UDP__close& send_par)
{
  log("entering UDPasp__PT::outgoing_send(ASP__UDP__close)");
  int close_fd=conn_list[send_par.id()].fd;
  FD_CLR(close_fd,&conn_map);
  conn_list[send_par.id()].status=0;
  num_of_conn--;
  Uninstall_Handler();
  if(num_of_conn) Install_Handler(&conn_map, NULL, NULL, 0.0);
  close(close_fd);
  log("leaving UDPasp__PT::outgoing_send(ASP__UDP__close)");
}

}
