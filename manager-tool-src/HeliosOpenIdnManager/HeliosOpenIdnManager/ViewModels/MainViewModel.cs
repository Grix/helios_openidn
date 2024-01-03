using Avalonia.Controls;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using IniParser;
using IniParser.Model;
using Renci.SshNet;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Reflection;
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
        [NotifyPropertyChangedFor(nameof(ShortServerSoftwareUpdatePath))]
        private string? _serverSoftwareUpdatePath;

        public string? ShortServerSoftwareUpdatePath => Path.GetFileName(ServerSoftwareUpdatePath);

        private IniData? rawSettings;


        public MainViewModel()
        {
            CurrentManagerVersion = Assembly.GetExecutingAssembly().GetName().Version!.ToString().Split('-')[0][0..^2];
            CheckForManagerUpdates();
            CheckForServerUpdates();
        }

        [RelayCommand]
        public void ScanForServers()
        {
            Servers.Clear();

            try
            {
                foreach (var ipAddress in OpenIdnUtilities.ScanForServers())
                {
                    try
                    {
                        if (OpenIdnUtilities.GetServerInfo(ipAddress) is not IdnServerInfo serverInfo)
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
                using var scpClient = OpenIdnUtilities.GetScpConnection(server.ServerInfo.IpAddress);
                scpClient.Connect();

                using var settingsStream = new MemoryStream();
                scpClient.Download("/home/laser/openidn/settings.ini", settingsStream);
                settingsStream.Position = 0;
                using var reader = new StreamReader(settingsStream);
                rawSettings = new StreamIniDataParser().ReadData(reader);

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
                WifiSsid = rawSettings["network"]["wifi_ssid"] ?? "";
                WifiPassword = rawSettings["network"]["wifi_password"] ?? "";
                var name = rawSettings["idn_server"]["name"];
                NewServerName = !string.IsNullOrEmpty(name) ? name : server.ServerInfo.Name;
            }
            catch (Exception ex)
            {
                ErrorMessage = "Couldn't load current settings: " + ex.Message;
            }
        }

        [RelayCommand]
        public void LoadDefaultConfig()
        {
            NewServerName = "OpenIDN";
            EthernetIsDhcp = true;
            EthernetIpAddress = "";
            WifiIsDhcp = true;
            WifiIpAddress = "";
            WifiSsid = "";
            WifiPassword = "";
        }

        [RelayCommand]
        public void SaveAndApplyConfig()
        {
            UpdateRawSettingsData();

            try
            {
                var server = Servers[SelectedServerIndex];

                using var scpClient = OpenIdnUtilities.GetScpConnection(server.ServerInfo.IpAddress);
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
                if (newVersion != null && newVersion.Length < 20 && newVersion != currentVersion)
                    NewManagerVersion = newVersion;
                else
                    NewManagerVersion = null;
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
                var currentVersion = "0.9.0"; // todo get from server
                if (newVersion != null && newVersion.Length < 20 && newVersion != currentVersion)
                    NewServerVersion = newVersion;
                else
                    NewServerVersion = null;
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

                using var sshClient = OpenIdnUtilities.GetSshConnection(server.ServerInfo.IpAddress);
                sshClient.Connect();
                sshClient.RunCommand("pkill helios_openidn");
                sshClient.RunCommand("mv /home/laser/openidn/helios_openidn /home/laser/openidn/helios_openidn_backup");

                using var scpClient = OpenIdnUtilities.GetScpConnection(server.ServerInfo.IpAddress);
                scpClient.Connect();
                scpClient.Upload(new FileInfo(ServerSoftwareUpdatePath), "/home/laser/openidn/helios_openidn");
                sshClient.RunCommand("chmod +x /home/laser/openidn/helios_openidn");

                RestartServer();
            }
            catch (Exception ex)
            {
                ErrorMessage = "Couldn't update server software: " + ex.Message;
            }
        }

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
                var response = await client.GetAsync($"https://github.com/Grix/helios_openidn/releases/download/v{NewServerVersion}/helios_openidn_v{NewServerVersion}.bin");
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
                FileName = "https://bitlasers.com/openidn-network-adapter-for-the-helios-dac/#manager-tool",
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

                using var sshClient = OpenIdnUtilities.GetSshConnection(server.ServerInfo.IpAddress);
                sshClient.Connect();
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
            using var sshClient = OpenIdnUtilities.GetSshConnection(server.ServerInfo.IpAddress);

            try
            {
                sshClient.Connect(); 
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
                var command = sshClient.RunCommand(OpenIdnUtilities.GetSudoSshCommand("reboot"));
            }
            catch { }
        }

        void UpdateRawSettingsData()
        {
            rawSettings ??= new();

            if (EthernetIsDhcp)
                rawSettings["network"]["ethernet_ip_addresses"] = "auto";
            else
                rawSettings["network"]["ethernet_ip_addresses"] = EthernetIpAddress;

            if (WifiIsDhcp)
                rawSettings["network"]["wifi_ip_addresses"] = "auto";
            else
                rawSettings["network"]["wifi_ip_addresses"] = WifiIpAddress;

            rawSettings["network"]["wifi_ssid"] = WifiSsid;
            rawSettings["network"]["wifi_password"] = WifiPassword;
            rawSettings["idn_server"]["name"] = NewServerName;

            if (rawSettings["network"].ContainsKey("already_applied"))
                rawSettings["network"].RemoveKey("already_applied"); // Otherwise it skips network settings

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

            var entries = value.Split(' ');

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
    }
}
