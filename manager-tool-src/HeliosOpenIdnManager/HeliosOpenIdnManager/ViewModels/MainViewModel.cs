using Avalonia.Controls;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using IniParser;
using IniParser.Model;
using IniParser.Parser;
using Material.Icons.Avalonia;
using Renci.SshNet;
using Renci.SshNet.Sftp;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Sockets;
using System.Reflection;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace HeliosOpenIdnManager.ViewModels
{
    public partial class MainViewModel : ViewModelBase
    {
        [ObservableProperty]
        private string _selectedServerTitle = "";

        [ObservableProperty]
        private string _servicesListString = "";

        [ObservableProperty]
        private string _newServerName = "";

        [ObservableProperty]
        private string _ethernetIpAddress = "";

        [ObservableProperty]
        private bool _ethernetIsDhcp = true;

        [ObservableProperty]
        private bool _wifiIsEnabled = true;

        [ObservableProperty]
        private string _wifiIpAddress = "";

        [ObservableProperty]
        private bool _wifiIsDhcp = true;

        [ObservableProperty]
        private string _wifiSsid = "";

        [ObservableProperty]
        private string _wifiPassword = "";

        [ObservableProperty]
        private bool _ethernetIpAddressHasWarning = false;

        [ObservableProperty]
        private bool _wifiIpAddressHasWarning = false;

        [ObservableProperty]
        [NotifyPropertyChangedFor(nameof(HasSelectedServer))]
        private int _selectedServerIndex = -1;

        public bool HasSelectedServer => SelectedServerIndex >= 0 || Design.IsDesignMode;

        [ObservableProperty]
        private ObservableCollection<IdnServerViewModel> _servers = new();

        [ObservableProperty]
        private ObservableCollection<string> _wifiNetworks = new();

        [ObservableProperty]
        private int _selectedWifiNetworkIndex = -1;

        [ObservableProperty]
        private uint _bufferLengthMs = 40;

        [ObservableProperty]
        private string? _errorMessage;

        [ObservableProperty]
        private string? _newManagerVersion;

        [ObservableProperty]
        private string? _newServerVersion;

        [ObservableProperty]
        private string? _currentManagerVersion;

        [ObservableProperty]
        private string? _currentServerVersion;

        [ObservableProperty]
        private int _networkModePriority = 4;

        [ObservableProperty]
        private int _usbModePriority = 3;

        [ObservableProperty]
        private int _dmxModePriority = 2;

        [ObservableProperty]
        private int _fileModePriority = 1;

        [ObservableProperty]
        private string? _filePlayerStartingFilename;

        [ObservableProperty]
        private bool _filePlayerAutoplay;

        [ObservableProperty]
        private int _filePlayerModeIndex = 3;

        [ObservableProperty]
        private bool _filePlayerHandleMissingPrg = true;

        [ObservableProperty]
        //[NotifyPropertyChangedFor(nameof(FilesStrings))]
        private ObservableCollection<HeliosFileViewModel> _files = new();

        //List<string> FilesStrings => new List<string>() { "test1", "test2" };//Files.Select(item => item.ToString()).ToList();

        [ObservableProperty]
        private int _selectedFileIndex = -1;

        [ObservableProperty]
        private bool _fileListIsRefreshing = false;

        [ObservableProperty]
        private ObservableCollection<FileInfo> _filesToUpload = new();

        [ObservableProperty]
        [NotifyPropertyChangedFor(nameof(ShortServerSoftwareUpdatePath))]
        private string? _serverSoftwareUpdatePath;

        [ObservableProperty]
        private string? _mcuFirmwareUpdatePath;

        public string? ShortServerSoftwareUpdatePath => Path.GetFileName(ServerSoftwareUpdatePath);

        private IniData? rawSettings;

        const string remoteFileLibraryPath = "/home/laser/library/";
        const string remoteFileUsbPath = "/media/usbdrive/";

        public MainViewModel()
        {
            CurrentManagerVersion = Assembly.GetExecutingAssembly().GetName().Version!.ToString().Split('-')[0][0..^2];
            CheckForManagerUpdates();
            CheckForServerUpdates();

            LoadDefaultConfig();
        }

        [RelayCommand]
        public void ScanForServers()
        {
            Servers.Clear();

            try
            {
                foreach (var ipAddress in HeliosOpenIdnUtilities.ScanForServers())
                {
                    try
                    {
                        if (HeliosOpenIdnUtilities.GetServerInfo(ipAddress) is not IdnServerInfo serverInfo)
                            continue;

                        Servers.Add(new IdnServerViewModel(serverInfo));
                    }
                    catch (Exception ex)
                    {
                        ErrorMessage = $"Couldn't get info of server {ipAddress} when scanning for OpenIDN servers: " + ex.Message;
                    }
                }
            }
            catch (Exception ex)
            {
                ErrorMessage = "Couldn't scan for OpenIDN servers: " + ex.Message;
            }

            if (Servers.Count > 0)
                SelectedServerIndex = 0;
            else
                SelectedServerIndex = -1;
        }

        [RelayCommand]
        public void LoadCurrentConfig()
        {
            var server = Servers[SelectedServerIndex];

            try
            {
                // Try to get settings via UDP socket first
                bool success = false;
                try
                {
                    using var udpClient = new UdpClient();
                    var data = new byte[] { 0xE5, 0x4 };
                    udpClient.Client.ReceiveTimeout = 500;
                    udpClient.Client.SendTimeout = 500;

                    var sendAddress = new IPEndPoint(server.ServerInfo.IpAddress, 7355);
                    udpClient.Send(data, data.Length, sendAddress);

                    var receiveAddress = new IPEndPoint(IPAddress.Any, sendAddress.Port);
                    var receivedData = udpClient.Receive(ref receiveAddress);

                    if (receivedData is not null && receivedData.Length > 4 && receivedData[0] == 0xE5 + 1 && receivedData[1] == 0x4)
                    {
                        receivedData[receivedData.Length - 1] = 0;
                        var settings = Encoding.UTF8.GetString(receivedData, 2, receivedData.Length - 2);
                        //var stream = new MemoryStream();
                        //stream.Write(receivedData, 2, receivedData.Length - 2);
                        //using var reader = new StreamReader(stream);
                        //rawSettings = new StreamIniDataParser().ReadData(reader);
                        //var tempPath = Path.GetTempFileName();
                        //File.WriteAllText(tempPath, settings);
                        //rawSettings = new FileIniDataParser().ReadFile(tempPath);
                        var parser = new IniDataParser();
                        parser.Configuration.SkipInvalidLines = true;
                        rawSettings = parser.Parse(settings);
                        success = true;
                    }
                }
                catch { }

                // Use SCP transfer to get settings file if UDP socket fails
                if (!success)
                {
                    using var scpClient = HeliosOpenIdnUtilities.GetScpConnection(server.ServerInfo.IpAddress);
                    scpClient.Connect();

                    using var settingsStream = new MemoryStream();
                    scpClient.Download("/home/laser/openidn/settings.ini", settingsStream);
                    settingsStream.Position = 0;
                    using var reader = new StreamReader(settingsStream);
                    rawSettings = new StreamIniDataParser().ReadData(reader);
                }

                if (rawSettings == null)
                    throw new Exception("No settings");

                var ethernetIpAddr = rawSettings["network"]["ethernet_ip_addresses"] ?? "";
                if (ethernetIpAddr == "auto")
                {
                    EthernetIsDhcp = true;
                    EthernetIpAddress = "";
                }
                else if (ethernetIpAddr != "")
                {
                    EthernetIsDhcp = false;
                    EthernetIpAddress = ethernetIpAddr;
                }

                WifiPassword = rawSettings["network"]["wifi_password"] ?? "";
                WifiSsid = rawSettings["network"]["wifi_ssid"] ?? "";

                var wifiEnable = rawSettings["network"]["wifi_enable"] ?? ((WifiSsid != "" && WifiPassword != "") ? "true" : "false");
                WifiIsEnabled = (wifiEnable.ToLower() == bool.TrueString.ToLower());

                var wifiIpAddr = rawSettings["network"]["wifi_ip_addresses"] ?? "";
                if (wifiIpAddr == "auto")
                {
                    WifiIsDhcp = true;
                    WifiIpAddress = "";
                }
                else if (wifiIpAddr != "")
                {
                    WifiIsDhcp = false;
                    WifiIpAddress = wifiIpAddr;
                }
                var name = rawSettings["idn_server"]["name"];
                NewServerName = !string.IsNullOrEmpty(name) ? name.Trim('"') : server.ServerInfo.Name;

                if (uint.TryParse((rawSettings["mode_priority"]["idn"] ?? "").Trim('"'), out uint idnModePriority))
                    NetworkModePriority = (int)idnModePriority;
                else
                    NetworkModePriority = 4;

                if (uint.TryParse((rawSettings["mode_priority"]["usb"] ?? "").Trim('"'), out uint usbModePriority))
                    UsbModePriority = (int)usbModePriority;
                else
                    UsbModePriority = 3;

                if (uint.TryParse((rawSettings["mode_priority"]["dmx"] ?? "").Trim('"'), out uint dmxModePriority))
                    DmxModePriority = (int)dmxModePriority;
                else
                    DmxModePriority = 2;

                if (uint.TryParse((rawSettings["mode_priority"]["file"] ?? "").Trim('"'), out uint fileModePriority))
                    FileModePriority = (int)fileModePriority;
                else
                    FileModePriority = 1;

                FilePlayerAutoplay = (rawSettings["file_player"]["autoplay"] ?? "false").ToLower().Trim('"') == "true";
                FilePlayerHandleMissingPrg = (rawSettings["file_player"]["handle_missing_prg"] ?? "true").ToLower().Trim('"') == "true";
                FilePlayerStartingFilename = (rawSettings["file_player"]["starting_file"] ?? "").Trim('"');
                var fileplayerMode = (rawSettings["file_player"]["mode"] ?? "").ToLower().Trim('"');
                if (FilePlayerModes.Contains(fileplayerMode))
                    FilePlayerModeIndex = Array.IndexOf(FilePlayerModes, fileplayerMode);
                else
                    FilePlayerModeIndex = 3;

                if (uint.TryParse((rawSettings["output"]["buffer_duration"] ?? "").Trim('"'), out uint bufferDurationMs))
                    BufferLengthMs = bufferDurationMs;
                else
                    BufferLengthMs = 40;
            }
            catch (Exception ex)
            {
                ErrorMessage = "Couldn't load current settings: " + ex.Message;
            }
        }

        /*
         rawSettings["network"]["ethernet_ip_addresses"] = EthernetIsDhcp ? "auto" : EthernetIpAddress;
            rawSettings["network"]["wifi_enable"] = WifiEnable ? "true" : "false";
            rawSettings["network"]["wifi_ip_addresses"] = WifiIsDhcp ? "auto" : WifiIpAddress;
            rawSettings["network"]["wifi_ssid"] = WifiSsid;
            rawSettings["network"]["wifi_password"] = WifiPassword;
            rawSettings["idn_server"]["name"] = NewServerName;

            rawSettings["file_player"]["autoplay"] = FilePlayerAutoplay ? "true" : "false";
            rawSettings["file_player"]["starting_file"] = FilePlayerStartingFilename ?? "";
            rawSettings["file_player"]["mode"] = FilePlayerModes[FilePlayerModeIndex];

            rawSettings["mode_priority"]["idn"] = NetworkModePriority.ToString(CultureInfo.InvariantCulture);
            rawSettings["mode_priority"]["usb"] = UsbModePriority.ToString(CultureInfo.InvariantCulture);
            rawSettings["mode_priority"]["dmx"] = DmxModePriority.ToString(CultureInfo.InvariantCulture);
            rawSettings["mode_priority"]["file"] = FileModePriority.ToString(CultureInfo.InvariantCulture);*/

        [RelayCommand]
        public void LoadDefaultConfig()
        {
            NewServerName = "HeliosPRO";
            EthernetIsDhcp = true;
            EthernetIpAddress = "";
            WifiIsEnabled = false;
            WifiIsDhcp = true;
            WifiIpAddress = "";
            WifiSsid = "";
            WifiPassword = "";
            FilePlayerAutoplay = false;
            FilePlayerModeIndex = 3;
            FilePlayerStartingFilename = "";
            NetworkModePriority = 4;
            UsbModePriority = 3;
            DmxModePriority = 2;
            FileModePriority = 1;
            BufferLengthMs = 40;
        }

        [RelayCommand]
        public void SaveAndApplyConfig()
        {
            UpdateRawSettingsData();

            try
            {
                var server = Servers[SelectedServerIndex];

                using var scpClient = HeliosOpenIdnUtilities.GetScpConnection(server.ServerInfo.IpAddress);
                scpClient.Connect();

                using var settingsStream = new MemoryStream();
                using var writer = new StreamWriter(settingsStream);
                new StreamIniDataParser().WriteData(writer, rawSettings);
                writer.Flush();
                settingsStream.Position = 0;
                scpClient.Upload(settingsStream, "/home/laser/openidn/settings.ini");
            }
            catch (Exception ex)
            {
                ErrorMessage = "Couldn't apply settings: " + ex.Message;
            }
        }

        [RelayCommand]
        public void ExportConfig(string filePath)
        {
            UpdateRawSettingsData();

            try
            { 
                new FileIniDataParser().WriteFile(filePath, rawSettings);
            }
            catch (Exception ex)
            {
                ErrorMessage = "Couldn't export file: " + ex.Message;
            }
        }

        [RelayCommand]
        public async Task CheckForManagerUpdates()
        {
            try
            {
                using var client = new HttpClient();
                var newVersion = await client.GetStringAsync("https://raw.githubusercontent.com/Grix/helios_openidn/main/manager-tool-version");
                var currentVersion = CurrentManagerVersion;
                if (newVersion != null && newVersion.Length < 20)
                    NewManagerVersion = newVersion;
                else
                {
                    NewManagerVersion = null;
                    throw new Exception("Unexpected version value");
                }
            }
            catch (Exception ex)
            {
                ErrorMessage = "Couldn't check for manager tool update: " + ex.Message;
            }
        }

        [RelayCommand]
        public async Task CheckForServerUpdates()
        {
            try
            {
                using var client = new HttpClient();
                var newVersion = await client.GetStringAsync("https://raw.githubusercontent.com/Grix/helios_openidn/main/server-version");
                if (newVersion != null && newVersion.Length < 20)
                    NewServerVersion = newVersion;
                else
                {
                    NewServerVersion = null;
                    throw new Exception("Unexpected version value");
                }
            }
            catch (Exception ex)
            {
                ErrorMessage = "Couldn't check for server software update: " + ex.Message;
            }
        }

        [RelayCommand]
        public void UpdateServerSoftware()
        {
            try
            {
                if (ServerSoftwareUpdatePath == null)
                    throw new Exception("No software file chosen.");
                if (!File.Exists(ServerSoftwareUpdatePath) || !ShortServerSoftwareUpdatePath!.Contains("openidn"))
                    throw new Exception("Invalid software file");

                var server = Servers[SelectedServerIndex];

                //using var sshClient = HeliosOpenIdnUtilities.GetSshConnection(server.ServerInfo.IpAddress);
                var sshClient = server.SshClient ?? throw new Exception("Could not connect");
                //sshClient.Connect();
                sshClient.RunCommand("pkill helios_openidn");
                Thread.Sleep(100);
                sshClient.RunCommand("mv /home/laser/openidn/helios_openidn /home/laser/openidn/helios_openidn_backup");
                Thread.Sleep(100);

                using var scpClient = HeliosOpenIdnUtilities.GetScpConnection(server.ServerInfo.IpAddress);
                //scpClient.Connect();
                scpClient.Upload(new FileInfo(ServerSoftwareUpdatePath), "/home/laser/openidn/helios_openidn");
                Thread.Sleep(100);
                sshClient.RunCommand("chmod +x /home/laser/openidn/helios_openidn");

                RestartServer();
            }
            catch (Exception ex)
            {
                ErrorMessage = "Couldn't update server software: " + ex.Message;
            }
        }

        [RelayCommand]
        public void UpdateMcuFirmware()
        {
            try
            {
                if (McuFirmwareUpdatePath == null)
                    throw new Exception("No firmware file chosen.");
                if (!File.Exists(McuFirmwareUpdatePath) || !McuFirmwareUpdatePath!.Contains("heliospro_bufferchip"))
                    throw new Exception("Invalid firmware file");

                var server = Servers[SelectedServerIndex];

                var sshClient = server.SshClient ?? throw new Exception("Could not connect");
                //sshClient.Connect();

                using var scpClient = HeliosOpenIdnUtilities.GetScpConnection(server.ServerInfo.IpAddress);
                scpClient.Connect();
                scpClient.Upload(new FileInfo(McuFirmwareUpdatePath), "/home/laser/"+Path.GetFileName(McuFirmwareUpdatePath));
                Thread.Sleep(100);
                sshClient.RunCommand(HeliosOpenIdnUtilities.GetSudoSshCommand($"/home/laser/picberry -f dspic33ck -w {Path.GetFileName(McuFirmwareUpdatePath)} & "));
                Thread.Sleep(20000);

                RestartServer();
            }
            catch (Exception ex)
            {
                ErrorMessage = "Couldn't update server software: " + ex.Message;
            }
        }

        /*[RelayCommand]
        public void RandomizeMacAddress()
        {
            try
            {
                var server = Servers[SelectedServerIndex];

                using var sshClient = HeliosOpenIdnUtilities.GetSshConnection(server.ServerInfo.IpAddress);
                sshClient.Connect();
                sshClient.RunCommand($"nmcli connection modify \"Wired connection 1\" 802-3-ethernet.cloned-mac-address 72:E8:9E:{Random.Shared.Next() & 0xfe:X2}:{Random.Shared.Next() & 0xfe:X2}:{Random.Shared.Next() & 0xfe:X2}");
                if (!string.IsNullOrEmpty(WifiSsid))
                    sshClient.RunCommand($"nmcli connection modify \"{WifiSsid}\" 802-11-wireless.cloned-mac-address 0:10:17::{Random.Shared.Next() & 0xfe:X2}:{Random.Shared.Next() & 0xfe:X2}:{Random.Shared.Next() & 0xfe:X2}");

                RestartServer();
            }
            catch (Exception ex)
            {
                ErrorMessage = "Couldn't update server software: " + ex.Message;
            }
        }*/

        [RelayCommand]
        public async Task DownloadAndSelectNewestServerSoftware()
        {
            ServerSoftwareUpdatePath = null;
            if (NewServerVersion == null)
            {
                ErrorMessage = "Couldn't download new server software: Newest version unknown";
                return;
            }

            try
            {
                using var client = new HttpClient();
                var response = await client.GetAsync($"https://github.com/Grix/helios_openidn/releases/download/server-v{NewServerVersion}/helios_openidn_v{NewServerVersion}.bin");
                var path = Path.Combine(Path.GetTempPath(), $"helios_openidn_v{NewServerVersion}.bin");
                using (var fileStream = new FileStream(path, FileMode.Create))
                await response.Content.CopyToAsync(fileStream);
                ServerSoftwareUpdatePath = path;
            }
            catch (Exception ex)
            {
                ErrorMessage = "Couldn't download new server software: " + ex.Message;
            }
        }

        [RelayCommand]
        public void OpenManagerDownloadPage()
        {
            Process.Start(new ProcessStartInfo
            {
                FileName = "https://bitlasers.com/openidn-network-adapter-for-the-helios-dac#manager-tool",
                UseShellExecute = true
            });
        }

        [RelayCommand]
        public void OpenManagerHelpPage()
        {
            Process.Start(new ProcessStartInfo
            {
                FileName = "https://bitlasers.com/openidn-network-adapter-for-the-helios-dac#manager-tool",
                UseShellExecute = true
            });
        }

        [RelayCommand]
        public void ScanWifiNetworks()
        {
            WifiNetworks.Clear();

            try
            {
                var server = Servers[SelectedServerIndex];

                var sshClient = server.SshClient ?? throw new Exception("Could not connect");
                //sshClient.Connect();
                var command = sshClient.RunCommand("nmcli -t device wifi list");
                foreach (string line in command.Result.Split('\n'))
                {
                    if (string.IsNullOrEmpty(line))
                        continue;

                    var fields = line.Split(':');
                    WifiNetworks.Add(fields[7]);
                }
            }
            catch (Exception ex)
            {
                ErrorMessage = "Couldn't scan WiFi networks: " + ex.Message;
            }
        }

        [RelayCommand]
        public void RestartServer()
        {
            var server = Servers[SelectedServerIndex];
            var sshClient = server.SshClient ?? throw new Exception("Could not connect");

            try
            {
                //sshClient.Connect(); 
                SelectedServerIndex = -1;
                Servers.Remove(server);
            }
            catch (Exception ex)
            {
                ErrorMessage = "Couldn't restart server: " + ex.Message;
            }
            try
            {
                // This will fail anyway, so put it in its own try-catch block without error handling.
                var command = sshClient.RunCommand(HeliosOpenIdnUtilities.GetSudoSshCommand("reboot"));
            }
            catch { }
        }

        [RelayCommand]
        public void OpenDmxWebsite()
        {
            Process.Start(new ProcessStartInfo
            {
                FileName = "http://" + Servers[SelectedServerIndex].ServerInfo.IpAddress.ToString() + ":9090",
                UseShellExecute = true
            });
        }

        [RelayCommand]
        public async Task RefreshFileList()
        {
            if (FileListIsRefreshing)
                return;

            ErrorMessage = null;
            Files.Clear();
            FileListIsRefreshing = true;

            try
            {
                var server = Servers[SelectedServerIndex];
                var sftpClient = server.SftpClient ?? throw new Exception("Could not connect");
                //await sftpClient.ConnectAsync(CancellationToken.None);
                await ListDirectoryRecursively(sftpClient, remoteFileLibraryPath);
                await ListDirectoryRecursively(sftpClient, remoteFileUsbPath);
            }
            catch (Exception ex)
            {
                ErrorMessage = "Couldn't scan for files: " + ex.Message; 
            }
            finally
            {
                //OnPropertyChanged(nameof(FilesStrings));
                FileListIsRefreshing = false;
            }


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
                    if (extension == ".ild" || extension == ".ilda")
                    {
                        Files.Add(new HeliosFileViewModel(file.FullName));
                    }
                }
            }

        }


        [RelayCommand]
        public async Task UploadFiles()
        {
            ErrorMessage = null;
            try
            {
                var server = Servers[SelectedServerIndex];
                using var scpClient = HeliosOpenIdnUtilities.GetScpConnection(server.ServerInfo.IpAddress);
                await scpClient.ConnectAsync(CancellationToken.None);
                foreach (var file in FilesToUpload)
                {
                    scpClient.Upload(file, remoteFileLibraryPath + file.Name);
                }
            }
            catch (Exception ex)
            {
                ErrorMessage = "Couldn't upload files: " + ex.Message;
            }
        }

        void UpdateRawSettingsData()
        {
            rawSettings ??= new();

            
            rawSettings["network"]["ethernet_ip_addresses"] = EthernetIsDhcp ? "auto" : EthernetIpAddress;
            rawSettings["network"]["wifi_enable"] = WifiIsEnabled ? "true" : "false";
            rawSettings["network"]["wifi_ip_addresses"] = WifiIsDhcp ? "auto" : WifiIpAddress;
            rawSettings["network"]["wifi_ssid"] = WifiSsid;
            rawSettings["network"]["wifi_password"] = WifiPassword;
            rawSettings["idn_server"]["name"] = NewServerName;

            rawSettings["file_player"]["autoplay"] = FilePlayerAutoplay ? "true" : "false";
            rawSettings["file_player"]["starting_file"] = (FilePlayerStartingFilename ?? "").Trim('"');
            rawSettings["file_player"]["mode"] = FilePlayerModes[FilePlayerModeIndex];
            rawSettings["file_player"]["handle_missing_prg"] = FilePlayerHandleMissingPrg ? "true" : "false";

            rawSettings["mode_priority"]["idn"] = NetworkModePriority.ToString(CultureInfo.InvariantCulture);
            rawSettings["mode_priority"]["usb"] = UsbModePriority.ToString(CultureInfo.InvariantCulture);
            rawSettings["mode_priority"]["dmx"] = DmxModePriority.ToString(CultureInfo.InvariantCulture);
            rawSettings["mode_priority"]["file"] = FileModePriority.ToString(CultureInfo.InvariantCulture);

            rawSettings["output"]["buffer_duration"] = BufferLengthMs.ToString(CultureInfo.InvariantCulture).Trim(',');

            if (rawSettings["network"].ContainsKey("already_applied"))
                rawSettings["network"].RemoveKey("already_applied"); // Otherwise it skips applying network settings

            rawSettings.Configuration = new() { NewLineStr = "\n" }; // Since host computer is linux
        }

        partial void OnSelectedServerIndexChanged(int value)
        {
            if (value < 0 || value >= Servers.Count)
            {
                SelectedServerTitle = "";
                ServicesListString = "";
                CurrentServerVersion = "";
            }
            else
            {
                var server = Servers[SelectedServerIndex];
                SelectedServerTitle = server.Title;
                var servicesString = "";
                foreach (var service in server.ServerInfo.Services)
                {
                    servicesString += "\n - " + service;
                }
                ServicesListString = servicesString;
                CurrentServerVersion = server.SoftwareVersion;
                server.Connect();
                LoadCurrentConfig();
            }
        }

        partial void OnEthernetIpAddressChanged(string value)
        {
            EthernetIpAddressHasWarning = !IsIpAddressStringValid(value);
        }

        partial void OnWifiIpAddressChanged(string value)
        {
            WifiIpAddressHasWarning = !IsIpAddressStringValid(value);
        }

        partial void OnEthernetIsDhcpChanged(bool value)
        {
            if (true)
                EthernetIpAddressHasWarning = false;
        }

        partial void OnWifiIsDhcpChanged(bool value)
        {
            if (true)
                WifiIpAddressHasWarning = false;
        }

        partial void OnWifiSsidChanged(string value)
        {
            SelectedWifiNetworkIndex = WifiNetworks.IndexOf(value);
        }

        partial void OnSelectedWifiNetworkIndexChanged(int value)
        {
            if (value < 0 || value >= WifiNetworks.Count)
                return;

            WifiSsid = WifiNetworks[SelectedWifiNetworkIndex];
        }


        bool IsIpAddressStringValid(string value)
        {
            if (value == "")
                return true;

            var entries = value.Split(", ");

            foreach (var entry in entries)
            {
                if (!entry.Contains("/"))
                {
                    if (entries.Length > 1)
                        return false;
                    if (entry.Split('.').Length != 4)
                        return false;
                    if (!IPAddress.TryParse(entry, out _))
                        return false;
                }
                else
                {
                    var parts = entry.Split('/');
                    if (parts.Length != 2)
                        return false;
                    else if (parts[0].Split('.').Length != 4)
                        return false;
                    else if (!IPAddress.TryParse(parts[0], out _))
                        return false;
                    else if (!int.TryParse(parts[1], out int mask) || mask < 0 || mask > 32)
                        return false;
                }
            }

            return true;
        }

        public readonly string[] FilePlayerModes = { "next", "shuffle", "once", "repeat" };
    }
}
