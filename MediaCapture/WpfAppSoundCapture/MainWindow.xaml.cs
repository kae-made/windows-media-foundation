using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;


namespace WpfAppSoundCapture
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        ObservableCollection<WaveInCapsWrapper> waveInCaps = new ObservableCollection<WaveInCapsWrapper>();
        public MainWindow()
        {
            InitializeComponent();
            this.Loaded += MainWindow_Loaded;
        }

        private MicSoundRecorder recorder;
        private void MainWindow_Loaded(object sender, RoutedEventArgs e)
        {
            recorder = new MicSoundRecorder();
            recorder.WaveData += Recorder_WaveData;
            lbDevices.ItemsSource = waveInCaps;
        }

        CancellationTokenSource ctSource;
        private void Recorder_WaveData(object? sender, byte[] waveData)
        {
            ctSource.Cancel();
            var dialog = new SaveFileDialog();
            dialog.Filter = "wave file|*.wav";
            if (dialog.ShowDialog() == true)
            {
                using (var fs = new FileStream(dialog.FileName, FileMode.Create))
                {
                    using(var binaryWriter = new BinaryWriter(fs))
                    {
                        binaryWriter.Write(waveData);
                    }
                }
            }
            this.Dispatcher.Invoke(() =>
            {
                progRecord.Value = 0;
            });
        }

        private async void buttonStart_Click(object sender, RoutedEventArgs e)
        {
            int deviceId = WaveNativeAPI.WAVE_MAPPER;
            if (lbDevices.SelectedItem != null)
            {
                deviceId = ((WaveInCapsWrapper)lbDevices.SelectedItem).DeviceId;
            }
            tbDeviceNum.Text = $"{deviceId}";
            int recordSecs = int.Parse(tbRecordSecs.Text);
            int progDeltaMSec = 100;
            int progDeltaUnit = (10 * recordSecs) / progDeltaMSec;
            if (recorder.TryWaveInOpen(deviceId))
            {
                ctSource = new CancellationTokenSource();
                Task.Run(async () =>
                {
                    while (true)
                    {
                        Dispatcher.Invoke(() => { progRecord.Value += progDeltaUnit; });
                        await Task.Delay(progDeltaMSec);
                        if (ctSource.Token.IsCancellationRequested)
                        {
                            break;
                        }
                    }
                }, ctSource.Token);
                recorder.WaveInStart(recordSecs);
            }
        }

        private void buttonGetDevices_Click(object sender, RoutedEventArgs e)
        {
            waveInCaps.Clear();

            int num = WaveNativeAPI.waveInGetNumDevs();
            tbDeviceNum.Text = $"{num}";

            for(int id = 0; id < num; id++)
            {
                var wic = new WaveNativeAPI.WaveInCaps();

                int result = WaveNativeAPI.waveInGetDevCaps(id, ref wic, Marshal.SizeOf(typeof(WaveNativeAPI.WaveInCaps)));
                if (result == WaveNativeAPI.MMSYSERR_NOERROR)
                {
                    waveInCaps.Add(new WaveInCapsWrapper(id, wic));
                }
            }
        }
    }
}
