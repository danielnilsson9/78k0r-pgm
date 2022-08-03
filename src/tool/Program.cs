using _78k0r_pgm.Model;
using System;
using System.CommandLine;
using System.IO;

namespace _78k0r_pgm
{
    internal class Program
    {

        static Connection Connect(string comPort)
		{
            var con = new Connection(comPort);

            Console.WriteLine(comPort + " is Open");

            Console.WriteLine("Connecting Programmer");
            con.Connect();
            Console.WriteLine(String.Format("Connected to {0}, flash size is {1} bytes", con.DeviceId, con.FlashSize));

            return con;
        }

        static void Disconnect(Connection con)
		{
            con.Disconnect();
            Console.WriteLine("Disconnected");
        }

        static void EraseFlash(string comPort)
		{
			try
			{
                var con = Connect(comPort);

                Console.WriteLine("Erasing Flash");
                con.EraseFlash();

                Disconnect(con);
            }
            catch(Exception ex)
			{
                Console.WriteLine("Error: " + ex.Message);
			}
        }

        static void WriteFlash(string comPort, FileInfo file)
		{
            try
            {
                var con = Connect(comPort);

                Console.WriteLine("Reading HEX File");
                var hex = new IntelHexFile(file.FullName, con.FlashSize);
                var memory = hex.Read();

                Console.WriteLine("Erasing Flash");
                con.EraseFlash();

                Console.WriteLine("Writing Flash");

                DateTime start = DateTime.UtcNow;

                int bytesWritten = 0;
                con.WriteFlashBegin(0x00, con.FlashSize - 1);

                int bytesLeftToWrite = memory.Length;
                while (bytesLeftToWrite > 0)
                {
                    int bytesToWrite = Math.Min(256, bytesLeftToWrite);

                    con.WriteFlashData(memory, bytesWritten, bytesToWrite);

                    bytesLeftToWrite -= bytesToWrite;
                    bytesWritten += bytesToWrite;

                    DateTime end = DateTime.UtcNow;
                    TimeSpan timeDiff = end - start;

                    Console.Write("\rBytes Written: {0}/{1} ({2:0.00}s)", bytesWritten, memory.Length, timeDiff.TotalSeconds);
                }
                if (bytesWritten > 0)
				{
                    Console.WriteLine();
                }
               
                Console.WriteLine("Verifying");
                con.WriteFlashEnd();
                Console.WriteLine("OK");

                Disconnect(con);
            }
            catch (Exception ex)
            {
                Console.WriteLine("Error: " + ex.Message);
            }
        }

        static void Main(string[] args)
        {
            var rootCmd = new RootCommand(description: "Renesas 78K0R microcontroller programmer.");


            var comPortInput = new Option<string>(
                "--com",
                "Name of COM port to upload to where the 78k0r-pgm device is connected.");

            var hexFileInput = new Option<FileInfo?>(
                "--input",
                "Path to file in intel hex format to flash to microcontroller.");

            var eraseFlag = new Option<bool>(
                "--erase",
                "Erase the entire flash memory of the microcontroller."
            );

            rootCmd.AddOption(comPortInput);
            rootCmd.AddOption(hexFileInput);
            rootCmd.AddOption(eraseFlag);

            rootCmd.SetHandler((comPort, input, erase) =>
            {
                if (erase)
				{
                    EraseFlash(comPort);
				}
				else if (input != null)
				{
                    WriteFlash(comPort, input);
                } 
            }, 
            comPortInput, hexFileInput, eraseFlag);

            rootCmd.Invoke(args);
        }

    }
}