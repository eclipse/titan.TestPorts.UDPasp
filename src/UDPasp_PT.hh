/******************************************************************************
* Copyright (c) 2000-2018 Ericsson Telecom AB
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
//  File:               UDPasp_PT.hh
//  Description:        UDP test port header
//  Rev:                R8A
//  Prodnr:             CNL 113 346
//


#ifndef UDPasp__PT_HH
#define UDPasp__PT_HH

#include <TTCN3.hh>
#include "UDPasp_Types.hh"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace UDPasp__Types {
  class ASP__UDP;
  class ASP__UDP__message;
  class ASP__UDP__open;
  class ASP__UDP__open_result;
  class ASP__UDP__close;
}

namespace UDPasp__PortType {

class UDPasp__PT_PROVIDER : public PORT {
public:
  UDPasp__PT_PROVIDER(const char *par_port_name=NULL);
  ~UDPasp__PT_PROVIDER();

  void set_parameter(const char *parameter_name,
    const char *parameter_value);

  void Event_Handler(const fd_set *read_fds,
    const fd_set *write_fds, const fd_set *error_fds,
    double time_since_last_call);

protected:
  void user_map(const char *system_port);
  void user_unmap(const char *system_port);

  void user_start();
  void user_stop();

  void outgoing_send(const UDPasp__Types::ASP__UDP& send_par);
  void outgoing_send(const UDPasp__Types::ASP__UDP__message& send_par);
  void outgoing_send(const UDPasp__Types::ASP__UDP__open& send_par);
  void outgoing_send(const UDPasp__Types::ASP__UDP__close& send_par);
  
  virtual void incoming_message(const UDPasp__Types::ASP__UDP& incoming_par) = 0;
  virtual void incoming_message(const UDPasp__Types::ASP__UDP__message& incoming_par) = 0;
  virtual void incoming_message(const UDPasp__Types::ASP__UDP__open__result& incoming_par) = 0;
  
  void log(const char *fmt, ...);
  void logHex(const char *prompt, const OCTETSTRING& msg);
  void setUpSocket();
  void closeDownSocket();
  unsigned long getHostId(const char* destHostName);

private:
  bool debugging;
  bool port_mode;  // false: basic mode. Works like R1A02
                   // true: advanced mode. The new features are activated
  bool broadcast;
  
  struct conn_data{
    int fd;
    int port_num;
    int status;
    struct sockaddr_in remote_Addr;
  };
  
  conn_data *conn_list;
  int num_of_conn;
  int conn_list_length;
  struct sockaddr_in localAddr;
  int target_fd;
  fd_set conn_map;
};
}
#endif
