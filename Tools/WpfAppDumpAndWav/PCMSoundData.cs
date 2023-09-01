using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WpfAppDumpAndWav
{
    public class PCMSoundData
    {
        public uint RiffChunkSize { get; set; }
        public string Format { get; set; }
        public bool HasJunk { get; set; }
        public UInt32 JunkBytes { get; set; }
        public UInt32 FmtChunkBytes { get; set; }
        public UInt16 AudioFormat { get; set; }
        public UInt16 Channels { get; set; }
        public UInt32 SamplingFrequency { get; set; }
        public UInt32 BytesPerSecond { get; set; }
        public UInt16 BytesOfBlock { get; set; }
        public UInt16 BitsPerSample { get; set; }
        public UInt16 ExtParamsSize { get; set; }
        public byte[] ExtParams { get; set; }
        public UInt32 SubChunkSize { get; set; }
        public UInt32 NumOfRecord { get; set; }
        public List<List<int>> WaveData { get; set; }

        public PCMSoundData()
        {
            WaveData = new List<List<int>>();
        }

        public void Load(Stream dataStream)
        {
            byte[] buf2 = new byte[2];
            byte[] buf4 = new byte[4];

            int readSize = dataStream.Read(buf4, 0, buf4.Length);
            string data = System.Text.Encoding.UTF8.GetString(buf4);
            if (data != "RIFF")
            {
                throw new ArgumentOutOfRangeException();
            }
            readSize = dataStream.Read(buf4, 0, buf4.Length);
            if (readSize != buf4.Length)
            {
                throw new ArgumentOutOfRangeException();
            }
            RiffChunkSize = BitConverter.ToUInt32(buf4);

            readSize = dataStream.Read(buf4, 0, buf4.Length);
            data = System.Text.Encoding.UTF8.GetString(buf4);
            if (data != "WAVE")
            {
                throw new ArgumentOutOfRangeException();
            }
            Format = data;

            readSize = dataStream.Read(buf4, 0, buf4.Length);
            data = System.Text.Encoding.UTF8.GetString(buf4);
            if (data == "fmt ")
            {
                //
            }
            else if (data == "JUNK")
            {
                HasJunk = true;
                readSize = dataStream.Read(buf4, 0, buf4.Length);
                JunkBytes = BitConverter.ToUInt32(buf4);
                if (JunkBytes > 0)
                {
                    var tmpBuf = new byte[JunkBytes];
                    readSize = dataStream.Read(tmpBuf, 0, tmpBuf.Length);
                    if (readSize != tmpBuf.Length) { throw new ArgumentOutOfRangeException(); }
                    if ((JunkBytes % 2) != 0)
                    {
                        readSize = dataStream.Read(buf2, 0, 1);
                        if (readSize != 1)
                        {
                            throw new ArgumentOutOfRangeException();
                        }
                    }
                    readSize = dataStream.Read(buf4, 0, buf4.Length);
                    data = System.Text.Encoding.UTF8.GetString(buf4);
                    if (data != "fmt ")
                    {
                        throw new ArgumentOutOfRangeException();
                    }
                }
            }
            else
            {
                throw new ArgumentOutOfRangeException();
            }
            readSize = dataStream.Read(buf4, 0, buf4.Length);
            FmtChunkBytes = BitConverter.ToUInt32(buf4);

            readSize = dataStream.Read(buf2, 0, buf2.Length);
            AudioFormat = BitConverter.ToUInt16(buf2);

            readSize = dataStream.Read(buf2, 0, buf2.Length);
            Channels = BitConverter.ToUInt16(buf2);

            readSize = dataStream.Read(buf4, 0, buf4.Length);
            SamplingFrequency = BitConverter.ToUInt32(buf4);

            readSize = dataStream.Read(buf4, 0, buf4.Length);
            BytesPerSecond = BitConverter.ToUInt32(buf4);

            readSize = dataStream.Read(buf2, 0, buf2.Length);
            BytesOfBlock = BitConverter.ToUInt16(buf2);

            readSize = dataStream.Read(buf2, 0, buf2.Length);
            BitsPerSample = BitConverter.ToUInt16(buf2);

            if (FmtChunkBytes > 16)
            {

                readSize = dataStream.Read(buf2, 0, buf2.Length);
                ExtParamsSize = BitConverter.ToUInt16(buf2);
                if (ExtParamsSize > 0)
                {
                    ExtParams = new byte[ExtParamsSize];
                    readSize = dataStream.Read(ExtParams, 0, ExtParamsSize);
                }
            }

            readSize = dataStream.Read(buf4, 0, buf4.Length);
            data = System.Text.Encoding.UTF8.GetString(buf4);

            if (data != "data")
            {
                throw new ArgumentOutOfRangeException();
            }

            readSize = dataStream.Read(buf4, 0, buf4.Length);
            SubChunkSize = BitConverter.ToUInt32(buf4);

            NumOfRecord = SubChunkSize / Channels;
            int bytesOfSample = BitsPerSample / 8;
            NumOfRecord /= (UInt32)bytesOfSample;

            for (int i = 0; i < Channels; i++)
            {
                var dataUnits = new List<int>();
                WaveData.Add(dataUnits);
            }

            int readBytePerLine = bytesOfSample * Channels;

            for (int i = 0; i < NumOfRecord; i++)
            {
                for (int c = 0; c < Channels; c++)
                {
                    readSize = dataStream.Read(buf2, 0, bytesOfSample);
                    int frag = 0;
                    if (bytesOfSample == 1)
                    {
                        frag |= buf2[0];
                    }
                    else if (bytesOfSample == 2)
                    {
                        frag = BitConverter.ToInt16(buf2);
                    }
                    WaveData[c].Add(frag);
                }
            }
        }
    }
}
