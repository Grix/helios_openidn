using CommunityToolkit.Mvvm.ComponentModel;
using Renci.SshNet;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Net;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace HeliosOpenIdnManager.ViewModels
{
    public partial class IdnServerViewModel : ViewModelBase
    {
        [ObservableProperty]
        private string _title;

        public IdnServerInfo ServerInfo { get; }
        public string SoftwareVersion { get; }
        private SftpClient? sftpClient = null;
        public SftpClient? SftpClient { 
            get 
            {
                // Client is set in a background thread. Try to wait until it is set before returning it
                var timeoutTime = DateTime.Now.AddSeconds(7);
                while (sftpClient == null || !sftpClient.IsConnected)
                {
                    if (DateTime.Now > timeoutTime)
                        break;
                    Thread.Sleep(50);
                }
                return sftpClient;
            } 
            private set { sftpClient = value; } 
        }
        private SshClient? sshClient = null;
        public SshClient? SshClient
        {
            get
            {
                // Client is set in a background thread. Try to wait until it is set before returning it
                var timeoutTime = DateTime.Now.AddSeconds(7);
                while (sshClient == null || !sshClient.IsConnected)
                {
                    if (DateTime.Now > timeoutTime)
                        break;
                    Thread.Sleep(50);
                }
                return sshClient;
            }
            private set { sshClient = value; }
        }

        bool isConnecting = false;

        public IdnServerViewModel(IdnServerInfo serverInfo)
        {
            ServerInfo = serverInfo;
            Title = $"{serverInfo.Name} ({serverInfo.IpAddress})";

            SoftwareVersion = HeliosOpenIdnUtilities.GetSoftwareVersion(serverInfo.IpAddress);
        }

        public async void Connect()
        {
            if (isConnecting)
                return;

            isConnecting = true;

            await Task.Run(() =>
            {
                try
                {
                    SshClient = HeliosOpenIdnUtilities.GetSshConnection(ServerInfo.IpAddress);
                    SftpClient = HeliosOpenIdnUtilities.GetSftpConnection(ServerInfo.IpAddress);
                    SshClient.ConnectAsync(CancellationToken.None);
                    SftpClient.ConnectAsync(CancellationToken.None);
                }
                catch
                {
                    SshClient?.Disconnect();
                    SshClient = null;
                    SftpClient?.Disconnect();
                    SftpClient = null;
                }
            });

            isConnecting = false;
        }

        ~IdnServerViewModel()
        {
            SshClient?.Disconnect();
            SshClient = null;
            SftpClient?.Disconnect();
            SftpClient = null;
        }
    }
}
