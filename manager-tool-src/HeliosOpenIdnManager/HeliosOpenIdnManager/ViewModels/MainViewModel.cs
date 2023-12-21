using Avalonia.Controls;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using IniParser;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.IO;
using System.Threading.Tasks;

namespace HeliosOpenIdnManager.ViewModels
{
    public partial class MainViewModel : ViewModelBase
    {
        [ObservableProperty]
        private string selectedServerTitle = "";

        [ObservableProperty]
        private string servicesListString = "";

        [ObservableProperty]
        private string newServerName = "";

        [ObservableProperty]
        private string ethernetIpAddress = "";

        [ObservableProperty]
        private bool ethernetIsDhcp = true;

        [ObservableProperty]
        private bool wifiIsEnabled = true;

        [ObservableProperty]
        private string wifiIpAddress = "";

        [ObservableProperty]
        private bool wifiIsDhcp = true;

        [ObservableProperty]
        private string wifiSsid = "";

        [ObservableProperty]
        private string wifiPassword = "";

        [ObservableProperty]
        [NotifyPropertyChangedFor(nameof(HasSelectedServer))]
        private int selectedServerIndex = -1;

        public bool HasSelectedServer => SelectedServerIndex >= 0 || Design.IsDesignMode;

        [ObservableProperty]
        private ObservableCollection<IdnServerViewModel> servers = new();

        [ObservableProperty]
        private ObservableCollection<string> wifiNetworks = new();

        [ObservableProperty]
        private int selectedWifiNetworkIndex = -1;

        public MainViewModel()
        {

        }

        [RelayCommand]
        public void ScanForServers()
        {
            foreach (var ipAddress in OpenIdnUtilities.ScanForServers())
            {
                if (OpenIdnUtilities.GetServerInfo(ipAddress) is not IdnServerInfo serverInfo)
                    continue;

                Servers.Add(new IdnServerViewModel(serverInfo));
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

            using var scpClient = OpenIdnUtilities.GetScpConnection(server.ServerInfo.IpAddress);
            scpClient.Connect();

            var directory = Path.Combine(Path.GetTempPath(), "HeliosOpenIdnManager");
            if (!Path.Exists(directory))
                Directory.CreateDirectory(directory);

            using var settingsStream = new MemoryStream();
            scpClient.Download("/home/laser/openidn/settings.ini", settingsStream);
            settingsStream.Position = 0;
            // Todo: maybe keep the full file, not just extracted settings. This would allow for backwards/forwards-compatibility, custom fields, etc.
            using var reader = new StreamReader(settingsStream);
            var iniData = new StreamIniDataParser().ReadData(reader);

            var ethernetIpAddr = iniData["network"]["ethernet_ip_addresses"] ?? "";
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
            var wifiIpAddr = iniData["network"]["wifi_ip_addresses"] ?? "";
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
            WifiSsid = iniData["network"]["wifi_ssid"] ?? "";
            WifiPassword = iniData["network"]["wifi_password"] ?? "";
            NewServerName = iniData["idn_server"]["name"] ?? server.ServerInfo.Name;
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

        }

        [RelayCommand]
        public void ScanWifiNetworks()
        {
            WifiNetworks.Clear();

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

        partial void OnSelectedServerIndexChanged(int value)
        {
            if (value < 0 || value >= Servers.Count)
            {
                SelectedServerTitle = "";
                ServicesListString = "";
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
            }
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
    }
}
