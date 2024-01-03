using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;
using System.Threading.Tasks;

namespace HeliosOpenIdnManager.ViewModels
{
    public partial class IdnServerViewModel : ViewModelBase
    {
        [ObservableProperty]
        private string _title;

        public IdnServerInfo ServerInfo { get; }
        public string SoftwareVersion { get; }

        public IdnServerViewModel(IdnServerInfo serverInfo)
        {
            ServerInfo = serverInfo;
            Title = $"{serverInfo.Name} ({serverInfo.IpAddress})";

            SoftwareVersion = OpenIdnUtilities.GetSoftwareVersion(serverInfo.IpAddress);
        }
    }
}
