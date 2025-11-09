using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HeliosOpenIdnManager.ViewModels
{
    public partial class HeliosFileViewModel : ViewModelBase
    {
        [ObservableProperty]
        [NotifyPropertyChangedFor(nameof(IsStoredInternally))]
        [NotifyPropertyChangedFor(nameof(FileName))]
        private string _filePath;

        public string FileName => FilePath.Split('/').Last();

        public bool IsStoredInternally => FilePath.StartsWith("/home");

        public HeliosFileViewModel(string filePath)
        {
            FilePath = filePath;
        }

        public override string ToString() => FileName;

        public static implicit operator string(HeliosFileViewModel obj) => obj.ToString();
    }
}
