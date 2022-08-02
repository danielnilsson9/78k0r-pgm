using System.IO.Ports;

namespace _78k0r_pgm.Model
{
	public class Connection
	{
		private enum Status
		{
			Ok = 0x00,
			ErrorTimeout = 0x01,
			ErrorErase = 0x02,
			ErrorWrite= 0x03,
			ErrorVerify = 0x04
		}

		private static byte Stx = 0xf1;
		private static byte Etx = 0xf2;

		private static byte CmdInit = 0x01;
		private static byte CmdReset = 0x02;
		private static byte CmdErase = 0x03;
		private static byte CmdWriteBegin = 0x04;
		private static byte CmdWriteData = 0x05;
		private static byte CmdWriteEnd = 0x06;

		private static byte RspStatus = 0x11;
		private static byte RspInit = 0x12;

		private SerialPort _port;

		public string DeviceId { get; private set; }

		public uint FlashSize { get; private set; }

		public uint BlockSize
		{
			get
			{
				return 1024;
			}
		}


		public Connection(string comPort)
		{
			DeviceId = "";
			_port = new SerialPort(comPort, 115200, Parity.None, 8, StopBits.One);
			_port.Handshake = Handshake.None;
			_port.DtrEnable = true; // Needed for USB CDC ?!
			_port.RtsEnable = true; // Needed for USB CDC ?!
			_port.ReadTimeout = 5000;
			_port.WriteTimeout = 5000;

			_port.Open();
		}

		public void Connect()
		{
			SendCommand(new byte[] { CmdInit });
			ReadInitResponse();
		}

		public void Disconnect()
		{
			SendCommand(new byte[] { CmdReset });
			ReadStatusResponse();

			_port.Close();
		}

		public void EraseFlash()
		{
			SendCommand(new byte[] { CmdErase });
			ReadStatusResponse();
		}


		public void WriteFlashBegin(uint startAddress, uint endAddress)
		{
			byte[] buf = new byte[7];
			buf[0] = CmdWriteBegin;
			buf[1] = (byte)(startAddress >> 16);
			buf[2] = (byte)(startAddress >> 8);
			buf[3] = (byte)(startAddress);
			buf[4] = (byte)(endAddress >> 16);
			buf[5] = (byte)(endAddress >> 8);
			buf[6] = (byte)(endAddress);

			SendCommand(buf);
			ReadStatusResponse();
		}

		public void WriteFlashData(byte[] buffer, int startIdx, int count)
		{
			byte[] buf = new byte[count + 1];
			buf[0] = CmdWriteData;
			Array.Copy(buffer, startIdx, buf, 1, count);

			SendCommand(buf);
			ReadStatusResponse();
		}

		public void WriteFlashEnd()
		{
			SendCommand(new byte[] { CmdWriteEnd });
			ReadStatusResponse();
		}


		private void SendCommand(byte[] cmd)
		{
			try
			{
				_port.Write(new byte[] { Stx, (byte)(cmd.Length >> 8), (byte)cmd.Length }, 0, 3);
				_port.Write(cmd, 0, cmd.Length);
				_port.Write(new byte[] { Etx }, 0, 1);		
			}
			catch(TimeoutException)
			{
				throw new TimeoutException("Write timeout occured.");
			}
		}

		private byte[] ReadResponse()
		{
			List<byte> resp = new List<byte>();

			int i = 0;
			uint len = 0;
			while (i < (len + 4))
			{
				try
				{
					int b = _port.ReadByte();
					if (b != -1)
					{
						if (i == 0 && b != Stx)
						{
							throw new InvalidDataException("Malformed response, unexpected start of message.");
						}
						else if (i == 1)
						{
							len |= (((uint)b) << 8);
						}
						else if (i == 2)
						{
							len |= (uint)b;
						}
						else if (i == (len + 3))
						{
							if (b != Etx)
							{
								throw new InvalidDataException("Malformed response, unexpected end of message.");
							}
						}

						resp.Add((byte)b);
						i++;
					}
				}
				catch (TimeoutException)
				{
					throw new TimeoutException("Read timeout occured.");
				}
			}

			return resp.ToArray();
		}

		private void ReadStatusResponse()
		{
			var resp = ReadResponse();

			if (resp.Length != 6)
			{
				throw new InvalidDataException("Malformed response, unexpected length.");
			}

			if (resp[3] != RspStatus)
			{
				throw new InvalidDataException("Malformed response, unexpected type.");
			}

			CheckStatus(resp[4]);
		}

		private void ReadInitResponse()
		{
			var resp = ReadResponse();

			if (resp.Length != 19)
			{
				throw new InvalidDataException("Malformed response, unexpected length.");
			}

			if (resp[3] != RspInit)
			{
				throw new InvalidDataException("Malformed response, unexpected type.");
			}

			CheckStatus(resp[4]);

			DeviceId = System.Text.Encoding.UTF8.GetString(resp, 5, 10).Trim();
			FlashSize = ((uint)(resp[15]) << 16) | ((uint)(resp[16]) << 8) | (uint)resp[17];
		}

		private void CheckStatus(byte status)
		{
			switch((Status)status)
			{
				case Status.Ok:
					return;
				case Status.ErrorTimeout:
					throw new Exception("Operation Timeout.");
				case Status.ErrorErase:
					throw new Exception("Erase operation failed.");
				case Status.ErrorWrite:
					throw new Exception("Write operation failed.");
				case Status.ErrorVerify:
					throw new Exception("Verify operation failed.");
				default:
					throw new Exception("Unknown error: " + status.ToString());
			}
		}
	}
}
