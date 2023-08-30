using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace WpfAppSoundCapture
{
    public class MicSoundRecorder
    {
        public event EventHandler WaveOpen;
        public event EventHandler WaveClose;
        public event EventHandler<byte[]> WaveData;
        public RecorderState State { get; private set; } = RecorderState.Close;

        private WaveNativeAPI.DelegateWaveInProc waveProc;
        private IntPtr hwi = IntPtr.Zero;
        private WaveNativeAPI.WaveFormatEx waveFormat;
        private WaveNativeAPI.WaveHdr waveHdr;

        public MicSoundRecorder()
        {
            waveProc = new WaveNativeAPI.DelegateWaveInProc(WaveInProc);

            waveFormat = new WaveNativeAPI.WaveFormatEx();
            waveFormat.wFormatTag = WaveNativeAPI.WAVE_FORMAT_PCM;
            waveFormat.cbSize = 0;
            waveFormat.nChannels = 1;
            waveFormat.nSamplesPerSec = 11025;
            waveFormat.wBitsPerSample = 8;
            waveFormat.nBlockAlign = (short)(waveFormat.wBitsPerSample / 8 * waveFormat.nChannels);
            waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
        }

        public bool TryWaveInOpen(int deviceId)
        {
            if (State != RecorderState.Close)
            {
                return true;
            }

            // var result = WaveNativeAPI.waveInOpen(ref hwi, WaveNativeAPI.WAVE_MAPPER, ref waveFormat, waveProc, IntPtr.Zero, WaveNativeAPI.CALLBACK_FUNCTION);
            var result = WaveNativeAPI.waveInOpen(ref hwi, deviceId, ref waveFormat, waveProc, IntPtr.Zero, WaveNativeAPI.CALLBACK_FUNCTION);
            if (result == WaveNativeAPI.MMSYSERR_NOERROR)
            {
                return true;
            }

            return false;
        }

        public void WaveInStart(int recordSecond)
        {
            if (State == RecorderState.Close || State == RecorderState.Recording)
            {
                throw new InvalidOperationException($"State : {State}");
            }

            var dataSize = waveFormat.nAvgBytesPerSec * recordSecond;
            waveHdr = new WaveNativeAPI.WaveHdr();
            waveHdr.lpData = Marshal.AllocHGlobal(dataSize);
            waveHdr.dwBufferLength = dataSize;

            var cdwh = Marshal.SizeOf<WaveNativeAPI.WaveHdr>();
            WaveNativeAPI.waveInPrepareHeader(hwi, ref waveHdr, cdwh);
            WaveNativeAPI.waveInAddBuffer(hwi, ref waveHdr, cdwh);
            if (State == RecorderState.Open)
            {
                WaveNativeAPI.waveInStart(hwi);
            }

            State = RecorderState.Recording;
        }

        public void WaveInProc(IntPtr hwi, uint uMsg, IntPtr dwInstance, IntPtr dwParam1, IntPtr dwParam2)
        {
            switch (uMsg)
            {
                case WaveNativeAPI.WIM_OPEN:

                    State = RecorderState.Open;
                    WaveOpen?.Invoke(this, EventArgs.Empty);
                    break;
                case WaveNativeAPI.WIM_CLOSE:

                    State = RecorderState.Close;
                    WaveClose?.Invoke(this, EventArgs.Empty);
                    break;
                case WaveNativeAPI.WIM_DATA:

                    // var wh = Marshal.PtrToStructure<WaveNativeAPI.WaveHdr>(dwParam1);
                    State = RecorderState.Ready;
                    OnWaveData();
                    break;
                default:
                    break;
            }
        }

        private void OnWaveData()
        {
            var headerSize = 44;
            var dataSize = waveHdr.dwBufferLength + headerSize;
            var waveData = new byte[dataSize];

            Array.Copy(Encoding.ASCII.GetBytes("RIFF"), 0, waveData, 0, 4);
            Array.Copy(BitConverter.GetBytes((uint)(dataSize - 8)), 0, waveData, 4, 4);
            Array.Copy(Encoding.ASCII.GetBytes("WAVE"), 0, waveData, 8, 4);
            Array.Copy(Encoding.ASCII.GetBytes("fmt "), 0, waveData, 12, 4);
            Array.Copy(BitConverter.GetBytes((uint)16), 0, waveData, 16, 4);
            Array.Copy(BitConverter.GetBytes((ushort)(waveFormat.wFormatTag)), 0, waveData, 20, 2);
            Array.Copy(BitConverter.GetBytes((ushort)(waveFormat.nChannels)), 0, waveData, 22, 2);
            Array.Copy(BitConverter.GetBytes((uint)(waveFormat.nSamplesPerSec)), 0, waveData, 24, 4);
            Array.Copy(BitConverter.GetBytes((uint)(waveFormat.nAvgBytesPerSec)), 0, waveData, 28, 4);
            Array.Copy(BitConverter.GetBytes((ushort)(waveFormat.nBlockAlign)), 0, waveData, 32, 2);
            Array.Copy(BitConverter.GetBytes((ushort)(waveFormat.wBitsPerSample)), 0, waveData, 34, 2);
            Array.Copy(Encoding.ASCII.GetBytes("data"), 0, waveData, 36, 4);
            Array.Copy(BitConverter.GetBytes((uint)(waveHdr.dwBufferLength)), 0, waveData, 40, 4);
            Marshal.Copy(waveHdr.lpData, waveData, headerSize, waveHdr.dwBufferLength);

            WaveNativeAPI.waveInUnprepareHeader(hwi, ref waveHdr, Marshal.SizeOf<WaveNativeAPI.WaveHdr>());
            Marshal.FreeHGlobal(waveHdr.lpData);

            WaveData?.Invoke(this, waveData);
        }

        public void WaveInClose()
        {
            if (State == RecorderState.Close)
            {
                return;
            }

            if (State != RecorderState.Open)
            {
                WaveNativeAPI.waveInStop(hwi);
            }
            //WaveNativeAPI.waveInReset(_Hwi);
            WaveNativeAPI.waveInClose(hwi);
        }

        public enum RecorderState
        {
            Close, Open, Ready, Recording,
        }
    }

    public class WaveInCapsWrapper : INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler? PropertyChanged;

        private WaveNativeAPI.WaveInCaps instance;

        private int deviceId;

        public WaveInCapsWrapper(int deviceId, WaveNativeAPI.WaveInCaps target)
        {
            this.deviceId = deviceId;
            instance = target;
        }

        public int DeviceId
        {
            get { return deviceId; }
            set
            {
                deviceId = value;
                OnPropertyChanged(nameof(DeviceId));
            }
        }

        public int MId
        {
            get { return instance.wMid; }
        }
        public int PId
        {
            get { return instance.wPid; }
        }
        public int DriverVersion
        {
            get { return instance.vDriverVersion; }
        }
        public string Name
        {
            get { return instance.szPname; }
        }
        public int Formats
        {
            get { return instance.dwFormats; }
        }
        public int Channels
        {
            get { return instance.wChannels; }
        }

        private void OnPropertyChanged(string name)
        {
            if (PropertyChanged != null)
            {
                PropertyChanged(this, new PropertyChangedEventArgs(name));
            }
        }
    }
}
