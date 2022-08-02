namespace _78k0r_pgm.Model
{
	public class IntelHexFile
	{

		private string _path;
		private uint _memorySize;

		private class Record
		{
			public enum RecordType
			{
				Data,
				EndOfFile,
				ExtendedSegmentAddress,
				StartSegmentAddress,
				ExtendedLinearAddress,
				StartLinearAddress
			}

			public RecordType Type { get; set; }
			public int ByteCount { get; set; }
			public int Address { get; set; }
			public byte[] Bytes { get; set; }
			public int CheckSum { get; set; }

			public Record()
			{
				Bytes = new byte[0];
			}
		}

		public class MemoryBlock
		{
			public uint StartAddress { get; set; }

			public byte[] Data { get; set; }

			public MemoryBlock()
			{
				Data = new byte[0];
			}
		}


		public IntelHexFile(string path, uint memorySize)
		{
			_path = path;
			_memorySize = memorySize;
		}

		public byte[] Read()
		{
			var result = new byte[_memorySize];
			for (int i = 0; i < result.Length; i++)
			{
				result[i] = (byte)0xff;
			}

			var baseAddress = 0;
			var endOfFile = false;

			foreach (var r in ReadRecords())
			{
				var rec = ParseRecord(r);

				switch(rec.Type)
				{
					case Record.RecordType.Data:
					{
						var nextAddress = rec.Address + baseAddress;
						for (var i = 0; i < rec.ByteCount; i++)
						{
							if (nextAddress + i > _memorySize)
							{
								throw new IOException(
									string.Format("Trying to write to position {0} outside of memory boundaries ({1}).",
										nextAddress + i, _memorySize));
							}

							result[nextAddress + i] = rec.Bytes[i];
						}

						break;
					}
					case Record.RecordType.EndOfFile:
					{
						endOfFile = true;
						break;
					}
					case Record.RecordType.ExtendedSegmentAddress:
					{
						baseAddress = (rec.Bytes[0] << 8 | rec.Bytes[1]) << 4;
						break;
					}
					case Record.RecordType.ExtendedLinearAddress:
					{
						baseAddress = (rec.Bytes[0] << 8 | rec.Bytes[1]) << 16;
						break;
					}
				}
			}

			if (!endOfFile)
			{
				throw new IOException("No eof marker found.");
			}

			return result;
		}


		private List<string> ReadRecords()
		{
			List<string> records = new List<string>();

			string text = File.ReadAllText(_path);

			string record = "";

			foreach (char c in text)
			{
				if (c == ':')
				{
					if (record.Length > 0)
					{
						records.Add(record);
					}

					record = ":";
				}
				else if (Char.IsLetter(c) || Char.IsNumber(c))
				{
					record += c;
				}
			}

			record = record.Trim();
			if (record.Length > 0)
			{
				records.Add(record);
			}

			return records;
		}

		private static Record ParseRecord(string record)
		{
			if (record.Length < 11)
			{
				throw new IOException(string.Format("Invalid record '{0}', too short.", record));
			}

			if (!record.StartsWith(':'))
			{
				throw new IOException(
					string.Format("Invalid record {0}, must start with ':'.", record));
			}
			
			// Parse byte count
			var byteCount = Convert.ToInt32(record.Substring(1, 2), 16);
			var requiredRecordLength =
				1                   // colon
				+ 2                 // byte count
				+ 4                 // address
				+ 2                 // record type
				+ (2 * byteCount)   // data
				+ 2;                // checksum

			if (record.Length != requiredRecordLength)
			{
				throw new IOException(string.Format("Record '{0}' does not have required length of {1}.",
						record, requiredRecordLength));
			}
				
			// Parse address
			var address = Convert.ToInt32(record.Substring(3, 4), 16);

			// Parse record type
			var recTypeVal = Convert.ToInt32(record.Substring(7, 2), 16);
			if (!Enum.IsDefined(typeof(Record.RecordType), recTypeVal))
			{
				throw new IOException(
					string.Format("Invalid record type value found: '{0}'.", recTypeVal));
			}
			
			var recType = (Record.RecordType)recTypeVal;

			// Parse bytes
			var bytes = TryParseBytes(record.Substring(9, 2 * byteCount));

			// Parse checksum
			var checkSum = Convert.ToInt32(record.Substring(9 + (2 * byteCount), 2), 16);

			// Verify
			if (!VerifyChecksum(record, byteCount, checkSum))
			{
				throw new IOException(string.Format("Checksum for line {0} is incorrect.", record));
			}
				
			return new Record
			{
				ByteCount = byteCount,
				Address = address,
				Type = recType,
				Bytes = bytes,
				CheckSum = checkSum
			};
		}



		private static byte[] TryParseBytes(string hexData)
		{
			try
			{
				var bytes = new byte[hexData.Length / 2];
				var counter = 0;
				foreach (var hexByte in Split(hexData, 2))
				{
					bytes[counter++] = (byte)Convert.ToInt32(hexByte, 16);
				}
					
				return bytes;
			}
			catch (Exception ex)
			{
				throw new IOException(
					string.Format("Unable to extract bytes for '{0}'.", hexData),
					ex);
			}
		}

		private static bool VerifyChecksum(string line, int byteCount, int checkSum)
		{
			var allbytes = new byte[5 + byteCount];
			var counter = 0;

			foreach (var hexByte in Split(line.Substring(1, (4 + byteCount) * 2), 2))
			{
				allbytes[counter++] = (byte)Convert.ToInt32(hexByte, 16);
			}
				
			var maskedSumBytes = allbytes.Sum(x => (ushort)x) & 0xff;
			var checkSumCalculated = (byte)(256 - maskedSumBytes);

			return checkSumCalculated == checkSum;
		}
		
		private static IEnumerable<string> Split(string str, int chunkSize)
		{
			return Enumerable.Range(0, str.Length / chunkSize)
				.Select(i => str.Substring(i * chunkSize, chunkSize));
		}

	}
}
