using HeliosOpenIdnManager.ViewModels;
using Renci.SshNet;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Numerics;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace HeliosOpenIdnManager;

public class HeliosProUtilities
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

    const string remoteFileLibraryPath = "/home/laser/library/";
    const string remoteFileUsbPath = "/media/usbdrive/";

    /// <summary>
    /// Finds IP addresses of Helios-OpenIDN servers on the network. NB: Doesn't use official IDN Hello protocol, only special protocol for Helios, so it cannot find generic IDN servers.
    /// </summary>
    /// <returns>List of IPAddresses of Helios-OpenIDN devices.</returns>
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
                                var mask = unicastIpAddress.IPv4Mask.GetAddressBytes();
                                for (int i = 0; i < 4; i++)
                                {
                                    broadcastAddress[i] |= (byte)~mask[i]; // Broadcast only on this interface. Cannot use IPAddress.Broadcast because it throws an error on macOS.
                                }
                                var target = new IPEndPoint(new IPAddress(broadcastAddress), MANAGEMENT_PORT);

                                udpClient.Client.ReceiveTimeout = 300;
                                udpClient.Client.SendTimeout = 300;

                                try
                                {
                                    udpClient.Send(data, data.Length, target);
                                }
                                catch (Exception ex)
                                {
                                    //throw new Exception($"Failed sending UDP message to {target} on interface {unicastIpAddress.Address}", ex);
                                }

                                bool receivedOk = false;
                                var receiveAddress = new IPEndPoint(IPAddress.Any, MANAGEMENT_PORT);

                                var startTime = DateTime.Now;
                                while (startTime.AddSeconds(1) > DateTime.Now)
                                {
                                    try
                                    {
                                        receivedOk = false;

                                        var receivedData = udpClient.Receive(ref receiveAddress);

                                        if (receivedData is not null && receivedData.Length >= 2 && receivedData[0] == 0xE6)
                                        {
                                            receivedOk = true;
                                        }
                                    }
                                    catch (Exception ex)
                                    {
                                        // Normal to time out if sending to non-openidn server
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
    }

    /// <summary>
    /// Gets the names of services that an IDN server provides
    /// </summary>
    /// <param name="server">IP Address of an IDN server</param>
    /// <returns>Enumerable list of service names, or null if error occured such as server not being reachable</returns>
    static public IdnServerInfo? GetServerInfo(IPAddress server)
    {
        scanSequenceNumber++;
        using var udpClient = new UdpClient();
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
            string name = System.Text.Encoding.UTF8.GetString(receivedData[(8 + 16)..(8 + 16 + 20)]).Replace("\0", "");

            var services = GetServiceNamesOfServer(server).ToArray();

            return new IdnServerInfo() { Name = name, UnitId = unitId, Services = services, IpAddress = server };
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
        using var udpClient = new UdpClient();
        var data = new byte[] { IDNCMD_SERVICEMAP_REQUEST, 0, (byte)(scanSequenceNumber & 0xFF), (byte)((scanSequenceNumber >> 8) & 0xFF) };
        udpClient.Client.ReceiveTimeout = 500;
        udpClient.Client.SendTimeout = 500;

        var sendAddress = new IPEndPoint(server, IDN_HELLO_PORT);
        udpClient.Send(data, data.Length, sendAddress);

        var receiveAddress = new IPEndPoint(IPAddress.Any, sendAddress.Port);
        var receivedData = udpClient.Receive(ref receiveAddress);

        if (receivedData is not null && receivedData.Length >= 8 && receivedData[0] == IDNCMD_SERVICEMAP_REQUEST + 1 && (receivedData[2] | (receivedData[3] << 8)) == scanSequenceNumber)
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
                yield return System.Text.Encoding.UTF8.GetString(receivedData[(bufferPosition + 4)..(bufferPosition + 24)]).Replace("\0", "");
                bufferPosition += entrySize;
            }
        }
    }

    /// <summary>
    /// Gets the software version of a Helios-OpenIDN server. Specific to Helios-OpenIDN, not part of the IDN protocol.
    /// </summary>
    /// <param name="server"></param>
    /// <returns></returns>
    static public string GetSoftwareVersion(IPAddress server)
    {
        using var udpClient = new UdpClient();
        var data = new byte[] { 0xE5, 0x2 };
        udpClient.Client.ReceiveTimeout = 500;
        udpClient.Client.SendTimeout = 500;

        var sendAddress = new IPEndPoint(server, MANAGEMENT_PORT);
        udpClient.Send(data, data.Length, sendAddress);

        var receiveAddress = new IPEndPoint(IPAddress.Any, sendAddress.Port);
        var receivedData = udpClient.Receive(ref receiveAddress);

        if (receivedData is not null && receivedData.Length >= 10 && receivedData[0] == 0xE6 && receivedData[1] == 0x2)
        {
            return System.Text.Encoding.UTF8.GetString(receivedData[2..^0]).Replace("\0", "");
        }

        return "";
    }

    /// <summary>
    /// Uploads laser files (typically .ild and/or .prg) to the local internal storage on the device. Specific to Helios-OpenIDN, not part of the IDN protocol.
    /// </summary>
    /// <returns></returns>
    static public async Task UploadFiles(IPAddress server, IEnumerable<FileInfo> files)
    {
        using var scpClient = GetScpConnection(server);
        await scpClient.ConnectAsync(CancellationToken.None);
        foreach (var file in files)
        {
            scpClient.Upload(file, remoteFileLibraryPath + file.Name);
        }
    }

    static public async Task<List<ProgramViewModel>> GetProgramList(IPAddress server)
    {
        List<ProgramViewModel> programs = new();

        // Try to get file list via UDP socket first
        bool success = true;
        try
        {
            using var udpClient = new UdpClient();
            var data = new byte[] { 0xE5, 0x5 };
            udpClient.Client.ReceiveTimeout = 600;
            udpClient.Client.SendTimeout = 500;

            var sendAddress = new IPEndPoint(server, MANAGEMENT_PORT);
            await udpClient.SendAsync(data, data.Length, sendAddress);

            var receiveAddress = new IPEndPoint(IPAddress.Any, sendAddress.Port);
            var receivedData = udpClient.Receive(ref receiveAddress);

            if (receivedData is not null && receivedData.Length > 4 && receivedData[0] == 0xE5 + 1 && receivedData[1] == 0x5)
            {
                receivedData[receivedData.Length - 1] = 0;
                var fileListRaw = Encoding.UTF8.GetString(receivedData, 2, receivedData.Length - 2);

                var lines = fileListRaw.Split('\n');

                for (int i = 0; i < lines.Length; i++)
                {
                    var line = lines[i];
                    var programLineFields = line.Split(';');
                    if (programLineFields.Length >= 4)
                    {
                        ProgramViewModel program = new ProgramViewModel(programLineFields[0])
                        {
                            DmxIndex = int.Parse(programLineFields[1]),
                            IsStoredInternally = programLineFields[2] == "i",
                        };
                        int numIldaFiles = int.Parse(programLineFields[3]);
                        for (int ildaFileIndex = 0; ildaFileIndex < numIldaFiles; ildaFileIndex++)
                        {
                            i++;
                            var ildaFileLine = lines[i];
                            var ildaFileLineFields = ildaFileLine.Split(';');
                            if (ildaFileLineFields.Length >= 6)
                            {
                                program.IldaFiles.Add(new IldaFileViewModel(program,
                                                                            filename: ildaFileLineFields[0],
                                                                            speed: double.Parse(ildaFileLineFields[1], CultureInfo.InvariantCulture),
                                                                            speedType: int.Parse(ildaFileLineFields[2]),
                                                                            numRepetitions: uint.Parse(ildaFileLineFields[3]),
                                                                            palette: uint.Parse(ildaFileLineFields[4]),
                                                                            errorCode: int.Parse(ildaFileLineFields[5])));
                            }
                            else
                            {
                                success = false;
                                throw new Exception("Incorrectly formatted response: " + ildaFileLine);
                            }
                        }

                        programs.Add(program);
                    }
                    else if (programLineFields.Length > 1)
                    {
                        success = false;
                        throw new Exception("Incorrectly formatted response: " + line);
                    }
                }

            }
            else
                throw new Exception("No valid response");
        }
        catch (Exception ex)
        {
            throw new Exception("Couldn't scan for files: " + ex.Message, ex);
            success = false;
        }

        if (!success)
        {
            // TODO implement direct file scanning via sftp
            /*try
            {
                var sftpClient = GetSftpConnection(server) ?? throw new Exception("Could not connect");
                //await sftpClient.ConnectAsync(CancellationToken.None);
                await ListDirectoryRecursively(sftpClient, remoteFileLibraryPath);
                await ListDirectoryRecursively(sftpClient, remoteFileUsbPath);
            }
            catch (Exception ex)
            {
                throw new Exception("Couldn't scan for files: " + ex.Message, ex);
            }*/
        }

        return programs;


        async Task ListDirectoryRecursively(SftpClient client, string path)
        {
            await foreach (var file in client.ListDirectoryAsync(path, CancellationToken.None))
            {
                if (file.FullName == path || file.FullName.EndsWith('.'))
                    continue;

                if (file.IsDirectory)
                {
                    await ListDirectoryRecursively(client, file.FullName);
                }

                if (!file.IsRegularFile)
                    continue;

                var extension = Path.GetExtension(file.Name).ToLowerInvariant();
                // TODO parse PRG
                /*if (extension == ".ild" || extension == ".ilda")
                {
                    Programs.Add(new ProgramViewModel(file.FullName));
                }*/
            }
        }
    }

    /// <summary>
    /// Sets or updates existing program file list on the device, for example with changed parameters for ilda files, or new files altogether. 
    /// </summary>
    /// <param name="server"></param>
    /// <param name="updatedListString">Special command string for program data. Can be found with MainViewModel.AssembleProgramSetCommand()</param>
    static public void SetProgramList(IPAddress server, string updatedListString)
    {
        using var udpClient = new UdpClient();
        var data = new List<byte>(updatedListString.Length + 2) { 0xE5, 0x6 };
        data.AddRange(Encoding.UTF8.GetBytes(updatedListString));
        udpClient.Client.ReceiveTimeout = 500;
        udpClient.Client.SendTimeout = 500;

        var sendAddress = new IPEndPoint(server, MANAGEMENT_PORT);
        udpClient.Send(data.ToArray(), data.Count, sendAddress);
    }


    /// <summary>
    /// Create and connect an SSH connection to a Helios OpenIDN server
    /// </summary>
    /// <param name="hostname">IP Address of server</param>
    /// <returns>SSH Client (disposable), not yet connected.</returns>
    public static SshClient GetSshConnection(IPAddress hostname) => new SshClient(new ConnectionInfo(hostname.ToString(), sshUser, new PasswordAuthenticationMethod(sshUser, sshPassword)));

    public static ScpClient GetScpConnection(IPAddress hostname) => new ScpClient(new ConnectionInfo(hostname.ToString(), sshUser, new PasswordAuthenticationMethod(sshUser, sshPassword)));

    public static SftpClient GetSftpConnection(IPAddress hostname) => new SftpClient(new ConnectionInfo(hostname.ToString(), sshUser, new PasswordAuthenticationMethod(sshUser, sshPassword)));

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
