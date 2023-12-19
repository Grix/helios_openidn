using Avalonia.Controls;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using System;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Threading.Tasks;

namespace HeliosOpenIdnManager.ViewModels
{
    public partial class MainViewModel : ViewModelBase
    {
        [ObservableProperty]
        private string? selectedServerName;

        [ObservableProperty]
        private string? servicesListString;

        [ObservableProperty]
        private ObservableCollection<UserControl> serverListItems = new();

        public MainViewModel()
        {
        }

        [RelayCommand]
        public void ScanForServers()
        {
            foreach (var ipAddress in OpenIdnUtilities.ScanForServers())
            {
                SelectedServerName = ipAddress.ToString();
                var services = "";

                foreach (var service in OpenIdnUtilities.GetServiceNamesOfServer(ipAddress))
                    services += (Environment.NewLine + " - " + service);

                ServicesListString = services;

                using (var sshClient = OpenIdnUtilities.GetSshConnection(ipAddress))
                {
                    sshClient.Connect();
                    var command = sshClient.RunCommand("pwd");
                    Debug.WriteLine($"SSH pwd response: {command.Result}");
                }
            }
        }
    }
}
