using Avalonia;
using Avalonia.Styling;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
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

    [ObservableProperty]
    public bool _settingsAreChanged = false;

    public ProgramViewModel(string name)
    {
        Name = name;
    }

    public void InvalidateSettings() => SettingsAreChanged = true;

    public void IldaFileSettingsChanged(object? sender, EventArgs e)
    {
        SettingsAreChanged = true;
    }

    [RelayCommand]
    public async Task Save()
    {
        if (!SettingsAreChanged || MainViewModel.Singleton == null)
            return;

        SettingsAreChanged = false;

        try
        {
            var commandString = MainViewModel.AssembleProgramSetCommand([this]);
            HeliosProUtilities.SetProgramList(MainViewModel.Singleton.Servers[MainViewModel.Singleton.SelectedServerIndex].ServerInfo.IpAddress, commandString);
        }
        catch (Exception ex)
        {
            MainViewModel.Singleton.ErrorMessage = "Failed to save settings: " + ex.Message;
        }
    }

    public override string ToString() => Name;

    public static implicit operator string(ProgramViewModel obj) => obj.ToString();
}
