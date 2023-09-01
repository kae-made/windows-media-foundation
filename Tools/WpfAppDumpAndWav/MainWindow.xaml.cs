using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
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

namespace WpfAppDumpAndWav
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
        }
        List<byte[]> contents = new List<byte[]>();

        private void buttonOpen_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new OpenFileDialog();
            if (dialog.ShowDialog() == true)
            {
                tbFileName.Text = dialog.FileName;

                using (var fs = File.OpenRead(tbFileName.Text))
                {
                    fs.Seek(0, SeekOrigin.End);
                    long fileSize = fs.Position;
                    fs.Seek(0, SeekOrigin.Begin);
                    int readChunk = 16;
                    var buffer = new byte[readChunk];
                    int currentSize = 0;
                    contents.Clear();
                    while (currentSize < fileSize)
                    {
                        int readSize = fs.Read(buffer, 0, readChunk);
                        var content = new byte[readSize];
                        Array.Copy(buffer, 0, content, 0, readSize);
                        contents.Add(content);
                        currentSize += readSize;
                    }

                    var sb = new StringBuilder();
                    int lineIndex = 0;
                    foreach (var line in contents)
                    {
                        string asscii = "";
                        sb.Append(lineIndex.ToString("x8"));
                        int index = 0;
                        foreach (byte b in line)
                        {
                            if ((index % 4) == 0)
                            {
                                sb.Append(" ");
                            }
                            sb.Append(b.ToString("x2"));
                            if (0x20 <= b && b <= 0x7e)
                            {
                                asscii += (char)b;
                            }
                            else
                            {
                                asscii += ".";
                            }
                            index++;
                        }
                        sb.AppendLine($" {asscii}");
                        lineIndex += readChunk;
                    }
                    tbContent.Text = sb.ToString();

                    fs.Seek(0, SeekOrigin.Begin);
                    soundData = new PCMSoundData();
                    try
                    {
                        soundData.Load(fs);

                        ShowRIFFContent(soundData);
                        buttonExport.IsEnabled = true;
                    }
                    catch(Exception ex)
                    {
                        MessageBox.Show(ex.Message);
                    }
                }
            }
        }

        private PCMSoundData soundData;

        private void ShowRIFFContent(PCMSoundData soundData)
        {
            tbRiffId.Text = "RIFF";
            tbRiffSize.Text = $"{soundData.RiffChunkSize}";
            tbFormat.Text = soundData.Format;
            tbJunkSize.Text = $"{soundData.JunkBytes}";
            tbFmt.Text = "fmt";
            tbSizeOfFmt.Text = $"{soundData.FmtChunkBytes}";
            tbAudioFormat.Text = $"{soundData.AudioFormat}";
            tbChannels.Text = $"{soundData.Channels}";
            tbSamplingFreq.Text = $"{soundData.SamplingFrequency}";
            tbBytesPerSec.Text = $"{soundData.BytesPerSecond}";
            tbSizeOfBlock.Text = $"{soundData.BytesOfBlock}";
            tbBitsOfSample.Text = $"{soundData.BitsPerSample}";
            tbSubChunkIdentifier.Text = "data";
            tbSizeOfSubChunk.Text = $"{soundData.SubChunkSize}";
            tbNumOfRecords.Text = $"{soundData.NumOfRecord}";

            string paramExt = "";
            for(int i = 0; i < soundData.ExtParamsSize; i++)
            {
                paramExt += string.Format("x2", soundData.ExtParams[i]);
            }
            tbParamExt.Text = paramExt;
        }

        private void buttonExport_Click(object sender, RoutedEventArgs e)
        {
            var dialog = new SaveFileDialog();
            dialog.Filter = "CVS Files|*.cvs";
            if (dialog.ShowDialog() == true)
            {
                using (var stream = File.Create(dialog.FileName))
                {
                    using (var writer = new StreamWriter(stream))
                    {
                        writer.WriteLine($"RIFF Size,{soundData.RiffChunkSize}");
                        writer.WriteLine($"Format,{soundData.Format}");
                        writer.WriteLine($"Junk Size,{soundData.JunkBytes}");
                        writer.WriteLine($"fmt Chunk Bytes,{soundData.FmtChunkBytes}");
                        writer.WriteLine($"Audio Format,{soundData.AudioFormat}");
                        writer.WriteLine($"Channels,{soundData.Channels}");
                        writer.WriteLine($"Sampling Frequency,{soundData.SamplingFrequency}");
                        writer.WriteLine($"Bytes per Second,{soundData.BytesPerSecond}");
                        writer.WriteLine($"Bytes of Block,{soundData.BytesOfBlock}");
                        writer.WriteLine($"Bits of Sampling,{soundData.BitsPerSample}");
                        writer.WriteLine($"Bytes of Sub Chunk,{soundData.SubChunkSize}");
                        writer.WriteLine($"Number of Records,{soundData.NumOfRecord}");
                        for(var r = 0; r < soundData.NumOfRecord; r++)
                        {
                            string dataLine = "";
                            for (var c=0; c < soundData.Channels; c++)
                            {
                                if (!string.IsNullOrEmpty(dataLine))
                                {
                                    dataLine += ",";
                                }
                                dataLine += soundData.WaveData[c][r];
                            }
                            writer.WriteLine(dataLine);
                        }
                    }
                }
            }
        }
    }
}
