using CommunityToolkit.Mvvm.Input;
using System.Diagnostics;
using System.Threading.Tasks;

namespace HeliosOpenIdnManager.ViewModels
{
    public partial class MainViewModel : ViewModelBase
    {
        public MainViewModel()
        {
            ScanForServers();
        }

        [RelayCommand]
        public void ScanForServers()
        {
            foreach (var ipAddress in OpenIdnUtilities.ScanForServers())
            {
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
