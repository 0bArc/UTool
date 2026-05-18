using CsStratware.Infrastructure.Caching;
using Xunit;

namespace CsStratware.Tests;

public sealed class ContentHasherTests
{
    [Fact]
    public void HashText_is_stable()
    {
        var a = ContentHasher.HashText("hello");
        var b = ContentHasher.HashText("hello");
        Assert.Equal(a, b);
        Assert.Equal(64, a.Length);
    }
}
