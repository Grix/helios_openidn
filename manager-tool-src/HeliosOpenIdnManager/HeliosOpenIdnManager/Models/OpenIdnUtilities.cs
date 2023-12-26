using Renci.SshNet;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
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
    public const byte IDNCMD_SERVICEMAP_REQUEST = 0x12;

    static ushort scanSequenceNumber = 100;
    // Todo: support custom passwords
    static string sshUser = "laser";
    static string sshPassword = "pen_pineapple";
    static string sshSudoPassword = "pen_pineapple";

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

                                var data = new byte[] { 0xE5, 0x1 };
                                var broadcastAddress = unicastIpAddress.Address.GetAddressBytes();
                                broadcastAddress[3] = 255; // Broadcast only on this interface. Cannot use IPAddress.Broadcast because it throws an error on macOS.
                                var target = new IPEndPoint(new IPAddress(broadcastAddress), MANAGEMENT_PORT);

                                udpClient.Client.ReceiveTimeout = 300;
                                udpClient.Client.SendTimeout = 300;

                                try
                                {
                                    udpClient.Send(data, data.Length, target);
                                }
                                catch (Exception ex)
                                {
                                    throw new Exception($"Failed sending UDP message to {target} on interface {unicastIpAddress.Address}", ex);
                                }

                                var receiveAddress = new IPEndPoint(IPAddress.Any, MANAGEMENT_PORT);
                                bool receivedOk = false;
                                var receivedData = udpClient.Receive(ref receiveAddress);

                                if (receivedData is not null && receivedData.Length >= 2 && receivedData[0] == 0xE6)
                                {
                                    receivedOk = true;
                                }

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
    /// Gets the names of services that an IDN server provides
    /// </summary>
    /// <param name="server">IP Address of an IDN server</param>
    /// <returns>Enumerable list of service names, or null if error occured such as server not being reachable</returns>
    static public IdnServerInfo? GetServerInfo(IPAddress server)
    {
        scanSequenceNumber++;
        using (var udpClient = new UdpClient())
        {
            var data = new byte[] { IDNCMD_SCAN_REQUEST, 0, (byte)(scanSequenceNumber & 0xFF), (byte)((scanSequenceNumber >> 8) & 0xFF) };
            udpClient.Client.ReceiveTimeout = 500;
            udpClient.Client.SendTimeout = 500;

            var sendAddress = new IPEndPoint(server, IDN_HELLO_PORT);
            udpClient.Send(data, data.Length, sendAddress);

            var receiveAddress = new IPEndPoint(IPAddress.Any, sendAddress.Port);
            var receivedData = udpClient.Receive(ref receiveAddress);

            if (receivedData is not null && receivedData.Length >= 8 && receivedData[0] == IDNCMD_SCAN_REQUEST + 1 && (receivedData[2] | (receivedData[3] << 8)) == scanSequenceNumber)
            {
                var structSize = receivedData[4];
                var protocolVersion = receivedData[5];
                var status = receivedData[6];
                var reserved = receivedData[7];

                byte[] unitId = receivedData[8..(8 + 16)];
                string name = System.Text.Encoding.UTF8.GetString(receivedData[(8+16)..(8+16+20)]).Replace("\0", "");

                var services = GetServiceNamesOfServer(server).ToArray();

                return new IdnServerInfo() { Name = name, UnitId = unitId, Services = services, IpAddress = server };
            }
        }
        return null;
    }

    /// <summary>
    /// Gets the names of services that an IDN server provides
    /// </summary>
    /// <param name="server">IP Address of an IDN server</param>
    /// <returns>Enumerable list of service names. Empty if error occurs such as server not being reachable</returns>
    static public IEnumerable<string> GetServiceNamesOfServer(IPAddress server)
    {
        scanSequenceNumber++;
        using (var udpClient = new UdpClient())
        {
            var data = new byte[] { IDNCMD_SERVICEMAP_REQUEST, 0, (byte)(scanSequenceNumber & 0xFF), (byte)((scanSequenceNumber >> 8) & 0xFF) };
            udpClient.Client.ReceiveTimeout = 500;
            udpClient.Client.SendTimeout = 500;

            var sendAddress = new IPEndPoint(server, IDN_HELLO_PORT);
            udpClient.Send(data, data.Length, sendAddress);

            var receiveAddress = new IPEndPoint(IPAddress.Any, sendAddress.Port);
            var receivedData = udpClient.Receive(ref receiveAddress);

            if (receivedData is not null && receivedData.Length >= 8 && receivedData[0] == IDNCMD_SERVICEMAP_REQUEST+1 && (receivedData[2] | (receivedData[3] << 8)) == scanSequenceNumber)
            {
                var structSize = receivedData[4];
                var entrySize = receivedData[5];
                var relayCount = receivedData[6];
                var serviceCount = receivedData[7];

                int bufferPosition = 4 + structSize;

                for (int i = 0; i < relayCount; i++)
                {
                    bufferPosition += entrySize;
                }
                for (int i = 0; i < serviceCount; i++)
                {
                    yield return System.Text.Encoding.UTF8.GetString(receivedData[(bufferPosition + 4) .. (bufferPosition + 24)]).Replace("\0", "");
                    bufferPosition += entrySize;
                }
            }
        }
    }

    /// <summary>
    /// Create and connect an SSH connection to a Helios OpenIDN server
    /// </summary>
    /// <param name="hostname">IP Address of server</param>
    /// <returns>SSH Client (disposable), not yet connected.</returns>
    public static SshClient GetSshConnection(IPAddress hostname) => new SshClient(new ConnectionInfo(hostname.ToString(), sshUser, new PasswordAuthenticationMethod(sshUser, sshPassword)));

    public static ScpClient GetScpConnection(IPAddress hostname) => new ScpClient(new ConnectionInfo(hostname.ToString(), sshUser, new PasswordAuthenticationMethod(sshUser, sshPassword)));

    /// <summary>
    /// Transforms a raw SSH command into a command that applies sudo with the OpenIDN device's superuser password.
    /// </summary>
    /// <param name="command">SSH command that you want to run as sudo</param>
    /// <returns>SSH command that will run as sudo</returns>
    public static string GetSudoSshCommand(string command) => "echo \"" + sshSudoPassword + "\" | sudo -S " + command;

}

public struct IdnServerInfo
{
    public IPAddress IpAddress;
    public string Name;
    public byte[] UnitId;
    public string[] Services;
}
