using Renci.SshNet;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Numerics;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace HeliosOpenIdnManager;


public class OpenIdnUtilities
{
    public const int IDN_HELLO_PORT = 7255;
    public const int MANAGEMENT_PORT = 7355;
    public const byte IDNCMD_SCAN_REQUEST = 0x10;

    static ushort scanSequenceNumber = 100;

    /// <summary>
    /// Finds IP addresses of OpenIDN servers on the network.
    /// </summary>
    /// <returns>List of IPAddresses of OpenIDN devices.</returns>
    static public IEnumerable<IPAddress> ScanForServers()
    {
        foreach (NetworkInterface networkInterface in NetworkInterface.GetAllNetworkInterfaces())
        {
            if (networkInterface.OperationalStatus == OperationalStatus.Up && networkInterface.SupportsMulticast && networkInterface.GetIPProperties().GetIPv4Properties() != null)
            {
                int index = networkInterface.GetIPProperties().GetIPv4Properties().Index;
                if (NetworkInterface.LoopbackInterfaceIndex != index)
                {
                    foreach (UnicastIPAddressInformation unicastIpAddress in networkInterface.GetIPProperties().UnicastAddresses)
                    {
                        if (unicastIpAddress.Address.AddressFamily == AddressFamily.InterNetwork)
                        {
                            scanSequenceNumber++;

                            var localAddress = new IPEndPoint(unicastIpAddress.Address, 0);
                            using (var udpClient = new UdpClient(localAddress))
                            {
                                udpClient.Client.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.Broadcast, 1);
                                udpClient.Client.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.DontRoute, 1);

                                //var data = new byte[] { IDNCMD_SCAN_REQUEST, 0, (byte)(scanSequenceNumber & 0xFF), (byte)((scanSequenceNumber >> 8) & 0xFF) };
                                var data = new byte[] { 0xE5 };
                                var target = new IPEndPoint(IPAddress.Broadcast, MANAGEMENT_PORT);

                                udpClient.Client.ReceiveTimeout = 300;
                                udpClient.Client.SendTimeout = 300;

                                udpClient.Send(data, data.Length, target);

                                var receiveAddress = new IPEndPoint(IPAddress.Any, MANAGEMENT_PORT);
                                bool receivedOk = false;
                                try
                                {
                                    var receivedData = udpClient.Receive(ref receiveAddress);

                                    if (receivedData is not null && receivedData.Length > 0 && receivedData[0] == 0x12)
                                    {
                                        // todo: check SSH response / special packet to make sure it is OpenIDN and not other IDN servers
                                        receivedOk = true;
                                    }
                                }
                                catch (Exception ex) {}

                                if (receivedOk)
                                    yield return receiveAddress.Address;
                            }

                        }
                    }
                }
            }
        }
    }

    /// <summary>
    /// Create and connect an SSH connection to a Helios OpenIDN server
    /// </summary>
    /// <param name="hostname">IP Address of server</param>
    /// <returns>SSH Client (disposable), not yet connected.</returns>
    public static SshClient GetSshConnection(IPAddress hostname)
    {
        var client = new SshClient(new ConnectionInfo(hostname.ToString(), "laser", new PasswordAuthenticationMethod("laser", "pen_pineapple")));
        return client;
    }

}
