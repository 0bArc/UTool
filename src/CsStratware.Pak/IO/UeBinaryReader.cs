using System.Text;

namespace CsStratware.Pak.IO;

internal sealed class UeBinaryReader : BinaryReader
{
    public UeBinaryReader(Stream input) : base(input, Encoding.UTF8, leaveOpen: true)
    {
    }

    public string ReadFString()
    {
        var length = ReadInt32();
        if (length == 0)
            return string.Empty;

        if (length < 0)
        {
            var byteCount = -length * sizeof(char);
            var data = ReadBytes(byteCount);
            var chars = (byteCount / sizeof(char)) - 1;
            return Encoding.Unicode.GetString(data, 0, Math.Max(0, chars * sizeof(char)));
        }

        var ansi = ReadBytes(length);
        return Encoding.UTF8.GetString(ansi, 0, Math.Max(0, length - 1));
    }

    public T[] ReadTArray<T>(Func<T> readElement)
    {
        var count = ReadInt32();
        if (count <= 0)
            return [];

        var items = new T[count];
        for (var i = 0; i < count; i++)
            items[i] = readElement();
        return items;
    }
}
