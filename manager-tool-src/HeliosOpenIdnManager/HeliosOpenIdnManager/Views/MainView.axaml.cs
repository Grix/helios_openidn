using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Platform.Storage;
using HeliosOpenIdnManager.ViewModels;
using System.Collections.Generic;
using System.IO;
using System.Threading.Tasks;

namespace HeliosOpenIdnManager.Views
{
    public partial class MainView : UserControl
    {
        public MainView()
        {
            InitializeComponent();
        }

        private async void ExportConfigButton_Click(object sender, RoutedEventArgs e)
        {
            var file = await TopLevel.GetTopLevel(this)!.StorageProvider.SaveFilePickerAsync(new FilePickerSaveOptions{ Title = "Save Config File", SuggestedFileName = "settings.ini", DefaultExtension = ".ini" });

            if (file is not null)
            {
                var path = file.Path.AbsolutePath;
                if (!Path.HasExtension(path))
                    path = Path.ChangeExtension(path, ".ini");

                (DataContext as MainViewModel)!.ExportConfig(path);
            }
        }

        private async void SelectServerSoftwareButton_Click(object sender, RoutedEventArgs e)
        {
            var files = await TopLevel.GetTopLevel(this)!.StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions { Title = "Server Software Update"/*, FileTypeFilter = new List<FilePickerFileType>() { new FilePickerFileType("") }*/ });

            if (files is not null && files.Count == 1)
            {
                var path = files[0].Path.AbsolutePath;
                if (!path.Contains("openidn"))
                    (DataContext as MainViewModel)!.ErrorMessage = "Server software filename should contain 'openidn', are you sure this is a valid software update file?";
                else
                {
                    (DataContext as MainViewModel)!.ServerSoftwareUpdatePath = path;
                    (DataContext as MainViewModel)!.ErrorMessage = null;
                }
            }
        }
    }
}