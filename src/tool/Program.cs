using _78k0r_pgm.Model;
using System;
using System.CommandLine;

namespace _78k0r_pgm
{
    internal class Program
    {

        static Connection Connect(string comPort)
		{
            var con = new Connection(comPort);

            Console.WriteLine(comPort + " is Open");

            Console.WriteLine("Connecting to Programmer");
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
 
                uint numBlocks = (uint)memory.Length / con.BlockSize;

                int bytesWritten = 0;
                for (uint block = 0; block < numBlocks; ++block)
				{
                    con.WriteFlashBegin(block * con.BlockSize, ((block + 1) * con.BlockSize) - 1);

                    int bytesLeftInBlock = (int)con.BlockSize;
                    while (bytesLeftInBlock > 0)
                    {
                        int bytesToWrite = Math.Min(128, bytesLeftInBlock);

                        con.WriteFlashData(memory, bytesWritten, bytesToWrite);

                        bytesLeftInBlock -= bytesToWrite;
                        bytesWritten += bytesToWrite;

                        Console.WriteLine("Bytes Written: {0}/{1}", bytesWritten, memory.Length);
                    }

                    con.WriteFlashEnd();
                }

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