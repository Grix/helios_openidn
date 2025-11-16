using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HeliosOpenIdnManager.ViewModels;

public partial class ProgramViewModel : ViewModelBase
{
    [ObservableProperty]
    private string _name;

    [ObservableProperty]
    private bool _isStoredInternally;

    [ObservableProperty]
    private ObservableCollection<IldaFileViewModel> _ildaFiles = new();

    [ObservableProperty]
    private int _dmxIndex = -1;

    public ProgramViewModel(string name)
    {
        Name = name;
    }

    public override string ToString() => Name;

    public static implicit operator string(ProgramViewModel obj) => obj.ToString();
}
