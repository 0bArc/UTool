using System.Text;

namespace UTool.Pak.IO;

internal sealed class UeBinaryWriter : BinaryWriter
{
    public UeBinaryWriter(Stream output) : base(output, Encoding.UTF8, leaveOpen: true)
    {
    }

    public void WriteFString(string value)
    {
        if (string.IsNullOrEmpty(value))
        {
            Write(0);
            return;
        }

        var bytes = Encoding.UTF8.GetBytes(value);
        Write(bytes.Length + 1);
        Write(bytes);
        Write((byte)0);
    }

    public void WriteTArray<T>(IReadOnlyList<T> items, Action<T> writeElement)
    {
        Write(items.Count);
        foreach (var item in items)
            writeElement(item);
    }
}
