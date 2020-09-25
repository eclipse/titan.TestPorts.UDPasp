package org.eclipse.titan.titan_JavaTestPorts_UDP.user_provided;

import java.io.IOException;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.PortUnreachableException;
import java.nio.ByteBuffer;
import java.nio.channels.DatagramChannel;
import java.nio.channels.Pipe;
import java.nio.channels.SelectableChannel;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

import org.eclipse.titan.runtime.core.Optional;
import org.eclipse.titan.runtime.core.TTCN_Logger;
import org.eclipse.titan.runtime.core.TTCN_Logger.Severity;
import org.eclipse.titan.runtime.core.TitanCharString;
import org.eclipse.titan.runtime.core.TitanInteger;
import org.eclipse.titan.runtime.core.TitanOctetString;
import org.eclipse.titan.runtime.core.TitanPort;
import org.eclipse.titan.runtime.core.TtcnError;
import org.eclipse.titan.titan_JavaTestPorts_UDP.generated.UDPasp__Types;
import org.eclipse.titan.titan_JavaTestPorts_UDP.generated.UDPasp__Types.ASP__UDP;
import org.eclipse.titan.titan_JavaTestPorts_UDP.generated.UDPasp__Types.ASP__UDP__close;
import org.eclipse.titan.titan_JavaTestPorts_UDP.generated.UDPasp__Types.ASP__UDP__message;
import org.eclipse.titan.titan_JavaTestPorts_UDP.generated.UDPasp__Types.ASP__UDP__open;
import org.eclipse.titan.titan_JavaTestPorts_UDP.generated.UDPasp__Types.ASP__UDP__open__result;


public abstract class UDPasp__PT_PROVIDER extends TitanPort {

	private final String MODULE = "UDPasp__PT.";
	

	//private final int DEFAULT_NUM_CONN   = 10; unnecessary


	/**  port_mode
	 *     false: basic mode. Works like R1A02
	 *     true: advanced mode. The new features are activated
	 */
	private boolean port_mode = false;
	private boolean debugging = false;
	private int     localPort = 50000;
	private String  localAddress = null;

	DatagramChannel dc;

	HashMap<Integer,SelectableChannel> conn_list;

	Pipe pipe = null;
	Thread thread = null;

	//Constructor
	public UDPasp__PT_PROVIDER(String port_name) {
		super(port_name);
	}

	protected abstract void incoming_message(final UDPasp__Types.ASP__UDP__message incoming_par);
	protected abstract void incoming_message(final UDPasp__Types.ASP__UDP incoming_par);
	protected abstract void incoming_message(final UDPasp__Types.ASP__UDP__open__result incoming_par);
	
	
	//Set parameters
	@Override
	public void set_parameter(String parameter_name, String parameter_value) {
		log("Entering "+MODULE+"set_parameter: "+parameter_name+": "+parameter_value);
		if (parameter_name.equals("debugging")) {
			debugging = true;
		} else if (parameter_name.equals("mode")) {
			if (parameter_value.equals("advanced")) {
				port_mode = true;
			}
		} else if (parameter_name.equals("localIPAddr")) {
			localAddress = parameter_value;
		} else if (parameter_name.equals("localPort")) {
			try {
				localPort = Integer.parseInt(parameter_value);
			} catch (NumberFormatException nfe) {
				throw new TtcnError("Invalid localPort parameter: %s" + parameter_value);
			}
		} else if (parameter_name.equals("broadcast")) {
			TTCN_Logger.log_str(TTCN_Logger.Severity.WARNING_UNQUALIFIED, "Broadcast option is meaningless in java, the socket is broadcast-capable by defult");
		}
		else {
			TTCN_Logger.log_str(TTCN_Logger.Severity.WARNING_UNQUALIFIED, String.format("UDPasp__PT.set_parameter: Unsupported Test Port parameter: %s ", parameter_name));
		}
		log("Leaving "+MODULE+"set_parameter");
	}
	
	//Map, Unmap, Configuration
	@Override
	protected void user_map(String system_port) {
		log("Entering "+MODULE+"user_map");
		
		if (port_mode){ //Advanced mode
			if (conn_list != null) {
				throw new TtcnError("UDP Test Port ("+system_port+"): Internal error: conn_list is not NULL when mapping.");
			} else {
				conn_list = new HashMap<Integer,SelectableChannel>();
			}
		} else { //Simple config mode
			try {
				dc = DatagramChannel.open();
				dc.bind(new InetSocketAddress(localAddress, localPort));
				conn_list = new HashMap<Integer,SelectableChannel>();
				conn_list.put(new Integer(dc.hashCode()),(SelectableChannel)dc);

				Install_Handler(collectionToSet(conn_list.values()), null, 0.0);				
			} catch (Exception e) {
				// TODO: handle exception
				throw new TtcnError("Exception in userMap: Unable to set up socket" + e.getMessage());
			}
		}
		log("Leaving "+MODULE+"user_map");
	}

	@Override
	protected void user_unmap(String system_port) {
		log("Entering "+MODULE+"user_unmap");
		if (port_mode) {
			Iterator<SelectableChannel> it = conn_list.values().iterator();
			while (it.hasNext()) {
				dc = (DatagramChannel) it.next();
				try {
					if (dc.isOpen()) {
						dc.close();
					}
				} catch (IOException ioe) {
					throw new TtcnError("Exception in userUnmap: " + ioe.getMessage());
				}
			}
			conn_list = null;
		} else { //Unnecessary, since dc is added to readableSet just like in advanced mode
			try {
				dc.close();
				dc = null;
			} catch (IOException ioe) {
				throw new TtcnError("Exception in userUnmap: " + ioe.getMessage());
			}
			super.user_unmap(system_port);
		}
		try {
			Uninstall_Handler();
		} catch (IOException e) {
			throw new TtcnError("Exception in userUnmap (Uninstall_Handler failed): " + e.getMessage());
		}
		log("Leaving "+MODULE+"user_unmap");
	}
	
	@Override
	public void Handle_Event(SelectableChannel channel, boolean is_readable, boolean is_writeable) {
		log("Entering "+MODULE+"Handle_Event");
		if (!(channel instanceof DatagramChannel)) {
			throw new TtcnError("FATAL ERROR: incorrect channel received");
		}

		DatagramChannel source = ((DatagramChannel) channel);
		ByteBuffer buffer = ByteBuffer.allocate(65535);
		//CharBuffer cbuffer = CharBuffer.allocate(65535);
		try {
			//int bytesRead;
			InetSocketAddress remote = null;
			try {
				remote = (InetSocketAddress)source.receive(buffer);
			} catch (PortUnreachableException pue) {
				//Ignore for now, for more info see connect() function in 
				//https://docs.oracle.com/javase/7/docs/api/java/net/DatagramSocket.html
			}
			if (remote != null) {
				int received_length = buffer.position();
				byte[] received = new byte[received_length];
				buffer.rewind();
				buffer.get(received, 0, received_length);
				TitanOctetString incoming = new TitanOctetString(received);
				TitanCharString remote_address = new TitanCharString(remote.getHostString());

				TitanInteger remote_port = new TitanInteger(remote.getPort());
				if (port_mode) { //Advanced mode
					TitanInteger chId = new TitanInteger(channel.hashCode());

					Optional<TitanCharString> remote_address_optional = new Optional<TitanCharString>(TitanCharString.class);
					remote_address_optional.operator_assign(remote_address);

					Optional<TitanInteger> remote_port_optional = new Optional<TitanInteger>(TitanInteger.class);
					remote_port_optional.operator_assign(remote_port);

					Optional<TitanInteger> chId_optional = new Optional<TitanInteger>(TitanInteger.class);
					chId_optional.operator_assign(chId);

					ASP__UDP__message msg = new ASP__UDP__message(incoming, remote_address_optional, remote_port_optional, chId_optional); 

					incoming_message(msg);
				} else { //Simple config mode
					ASP__UDP msg = new ASP__UDP(incoming, remote_address, remote_port);
					incoming_message(msg);
				}
			}
		} catch (IOException e) {
			throw new TtcnError("IOException in HandleEvent: " + e.getMessage());
		}
		log("Leaving "+MODULE+"Handle_Event");
	}
	
	public void outgoing_send(ASP__UDP send_par) {
		log("Entering "+MODULE+"outgoing_send(ASP__UDP send_par)");
		InetSocketAddress address = null;

		//Check if the message has a valid address to be sent to
		TitanCharString addrf = send_par.constGet_field_addressf();
		if (addrf != null) {
			String hostname = addrf.get_value().toString();

			TitanInteger portf = send_par.constGet_field_portf();
			if (portf != null) {
				int port = portf.get_int();
				if ((hostname != null) && (hostname.length()>0) && (port > 0)){
					//TODO: Add more thorough checks for address and port in this condition
					//Overriding original send address & port
					address = new InetSocketAddress(hostname, port);
				}
			} 
		}
		try {
			TitanOctetString pdu = send_par.constGet_field_data();
			byte[] send_bytes = pdu.get_value();
			ByteBuffer bbToSend = ByteBuffer.wrap(send_bytes);
			dc.send(bbToSend,address);
		} catch (IOException ioe) {
			throw new TtcnError("IOException: " + ioe.getMessage());
		}
		log("Leaving "+MODULE+"outgoing_send(ASP__UDP send_par)");
	}

		
	public void outgoing_send(ASP__UDP__open send_par) {
		log("Entering "+MODULE+"outgoing_send(ASP__UDP send_par)");
		String localAddrStr;
		int localPort = 0;
		String remoteAddrStr;
		int remotePort = 0;

		InetSocketAddress localInetAddr  = null;
		InetSocketAddress remoteInetAddr = null;

		DatagramChannel dc;
		try {
			dc = DatagramChannel.open();
		} catch (IOException e) {
			throw new TtcnError("Can not open DatagramChannel: "+e.getMessage());
		}

		//Set local address if specified
		if (send_par.constGet_field_local__addr().is_present()) {
			localAddrStr = send_par.constGet_field_local__addr().get().get_value().toString();
			if (send_par.constGet_field_local__port().is_present()) {
				localPort = send_par.constGet_field_local__port().get().get_int();
			}
			try {
				localInetAddr = new InetSocketAddress(localAddrStr, localPort);
				dc.bind(localInetAddr);
			} catch (IOException ioe) {
				throw new TtcnError("IOException: " + ioe.getMessage());
			}
		}

		//Set remote address if specified
		if (send_par.constGet_field_remote__addr().is_present()) {
			remoteAddrStr = send_par.constGet_field_remote__addr().get().get_value().toString();
			if (send_par.constGet_field_remote__port().is_present()) {
				remotePort = send_par.constGet_field_remote__port().get().get_int();
				try {
					remoteInetAddr = new InetSocketAddress(InetAddress.getByName(remoteAddrStr), remotePort);
					dc.connect(remoteInetAddr);
				} catch (IOException ioe) {
					throw new TtcnError("IOException: " + ioe.getMessage());
				}
			}
		}

		//Add to conn_list
		int dcId = dc.hashCode();
		conn_list.put(dcId, dc);
		try {
			Uninstall_Handler();
			Install_Handler(collectionToSet(conn_list.values()), null, 0.0);
		} catch (IOException e) {
			throw new TtcnError("IOException: " + e.getMessage());
		}

		//Report result
		try {
			InetSocketAddress finalLocalAddress = (InetSocketAddress)dc.getLocalAddress();
			String finalLocalAddrStr = finalLocalAddress == null ? "" : finalLocalAddress.getHostString();
			int finalLocalPort       = finalLocalAddress == null ? 0  : finalLocalAddress.getPort();
			TitanCharString finalLocalAddrTitanStr = new TitanCharString(finalLocalAddrStr);
			TitanInteger    finalLocalTitanPort    = new TitanInteger(finalLocalPort);
			TitanInteger    id                     = new TitanInteger(dcId);
			ASP__UDP__open__result result = new ASP__UDP__open__result(finalLocalAddrTitanStr, finalLocalTitanPort, id);
			incoming_message(result);
		} catch (IOException e) {
			throw new TtcnError("IOException: " + e.getMessage());
		}
		log("Leaving "+MODULE+"outgoing_send(ASP__UDP__open send_par)");
	}

	
	public void outgoing_send(ASP__UDP__close send_par) {
		log("Entering "+MODULE+"outgoing_send(ASP__UDP__close send_par)");
		int dcId = send_par.constGet_field_id().get_int();
		DatagramChannel dc = (DatagramChannel) conn_list.remove(dcId);

		try {
			Uninstall_Handler();
			Install_Handler(collectionToSet(conn_list.values()), null, 0.0);
		} catch (IOException e1) {
			throw new TtcnError("IOException: " + e1.getMessage());
		}

		if (dc != null) {
			try {
				dc.close();
				dc = null;
			} catch (IOException e) {
				throw new TtcnError("IOException: " + e.getMessage());
			}
		} else {
			throw new TtcnError("Channel not found in conn_list");
		}
		log("Leaving "+MODULE+"outgoing_send(ASP__UDP__close send_par)");
	}

	
	public void outgoing_send(ASP__UDP__message send_par) {
		log("Entering "+MODULE+"outgoing_send(ASP__UDP__message send_par)");
		
		TitanOctetString pdu = send_par.constGet_field_data();
		TTCN_Logger.begin_event(Severity.ERROR_UNQUALIFIED);
		pdu.log();
		byte[] send_bytes = pdu.get_value();
		ByteBuffer bbToSend = ByteBuffer.wrap(send_bytes);
		TTCN_Logger.end_event();

		Optional<TitanInteger> titanId = send_par.constGet_field_id();
		if (titanId.is_present()) { //Channel ID is set
			DatagramChannel dc = (DatagramChannel) conn_list.get(titanId.get().get_int());
			try {
				dc.write(bbToSend);
			} catch (IOException e) {
				throw new TtcnError("IOException: " + e.getMessage());
			} catch (Exception ex) {
				throw new TtcnError("Exception when sending on DatagramChannel: " + ex.getMessage());
			}
			
		} else //Channel ID not set, "emulating" simple mode
		{
			try {
				dc = DatagramChannel.open();
				ASP__UDP asp_udp = new ASP__UDP(send_par.constGet_field_data(), send_par.constGet_field_remote__addr().get(), send_par.constGet_field_remote__port().get());
				outgoing_send(asp_udp);
				dc.close();
			} catch (IOException e) {
				throw new TtcnError("IOException: " + e.getMessage());
			}
		}
		log("Leaving "+MODULE+"outgoing_send(ASP__UDP__message send_par)");
	}

	private Set<SelectableChannel> collectionToSet(Collection<SelectableChannel> input) 
	{
		Set<SelectableChannel> output = new HashSet<SelectableChannel>();
		Iterator<SelectableChannel> it = input.iterator();
		while (it.hasNext()) {
			output.add(it.next());
		}
		return output;
	}
	
	private void log(String debugString) {
		if (debugging) TTCN_Logger.log_str(TTCN_Logger.Severity.DEBUG_TESTPORT, debugString);
	}

}
