using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading.Tasks;

namespace HeliosOpenIdnManager.ViewModels;

public partial class IldaFileViewModel : ViewModelBase
{
    [ObservableProperty]
    private string _fileName = "";

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(ParameterString))]
    private double _speed = 30;

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(ParameterString))]
    private int _speedType = 0;

    public string ParameterString => $"{Speed} {(SpeedType == 0 ? "kpps" : "FPS")}, {NumRepetitions} reps.";

    [ObservableProperty]
    private uint _numRepetitions = 1;

    [ObservableProperty]
    private uint _palette;

    [ObservableProperty]
    private int _errorCode;

    public event EventHandler SettingsChanged;

    bool ready = false;

    public IldaFileViewModel(ProgramViewModel parent, string filename, double speed, int speedType, uint numRepetitions, uint palette, int errorCode)
    {
        FileName = filename;
        Speed = speed;
        SpeedType = speedType;
        NumRepetitions = numRepetitions;
        Palette = palette;
        ErrorCode = errorCode;

        SettingsChanged += parent.IldaFileSettingsChanged;

        ready = true;
    }

    partial void OnErrorCodeChanged(int oldValue, int newValue)         
    {
        if (!ready)
            return;
        if (oldValue != newValue)
            SettingsChanged.Invoke(this, new EventArgs());
    }

    partial void OnSpeedTypeChanged(int oldValue, int newValue)         
    {
        if (!ready)
            return;
        if (newValue < 0 || newValue > 1)
        {
            SpeedType = 0;
            return;
        }

        if (oldValue != newValue)
            SettingsChanged.Invoke(this, new EventArgs());
    }

    partial void OnNumRepetitionsChanged(uint oldValue, uint newValue)  
    {
        if (!ready)
            return;
        if (newValue <= 0)
        {
            NumRepetitions = 1;
            return;
        }
        if (oldValue != newValue)
            SettingsChanged.Invoke(this, new EventArgs());
    }

    partial void OnPaletteChanged(uint oldValue, uint newValue)         
    {
        if (!ready)
            return;
        if (oldValue != newValue)
            SettingsChanged.Invoke(this, new EventArgs());
    }

    partial void OnSpeedChanged(double oldValue, double newValue)       
    {
        if (!ready)
            return;
        if (newValue < 0.1 || newValue > 100)
        {
            Speed = 30;
            return;
        }    

        if (oldValue != newValue)
            SettingsChanged.Invoke(this, new EventArgs());
    }
}
