using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HeliosOpenIdnManager.ViewModels;

public partial class IldaFileViewModel : ViewModelBase
{
    [ObservableProperty] 
    private string _fileName = "";

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(ParameterString))]
    private double _speed;

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(ParameterString))]
    private int _speedType;

    public string ParameterString => $"{Speed} {(SpeedType == 0 ? "kpps" : "FPS")}, {NumRepetitions} reps.";

    [ObservableProperty]
    private int _numRepetitions;

    [ObservableProperty]
    private int _palette;

    [ObservableProperty]
    private int _errorCode;
}
