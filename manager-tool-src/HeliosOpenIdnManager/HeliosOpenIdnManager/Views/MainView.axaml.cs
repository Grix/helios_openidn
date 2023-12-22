using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Platform.Storage;
using HeliosOpenIdnManager.ViewModels;
using System.IO;

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
    }
}