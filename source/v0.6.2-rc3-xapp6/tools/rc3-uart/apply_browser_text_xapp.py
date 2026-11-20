"""Move the existing bounded HTML converter into the external Browser."""

from pathlib import Path
import re
import sys


source = Path(sys.argv[1])
desktop = source / "desktop.cpp"
header = source / "desktop.h"
browser = source / "webtask.cpp"
text = desktop.read_text(encoding="utf-8")
declarations = header.read_text(encoding="utf-8")
http = browser.read_text(encoding="utf-8")

match = re.search(
    r"void Desktop::BrowserLoaded\(const char \*html,unsigned length\)\n\{\n"
    r"(?P<converter>.*?)"
    r"    browserLoading_=false;\n"
    r'    BrowserStatus\("HTTP 200 OK - text extracted"\);\n\}',
    text,
    re.DOTALL,
)
if not match:
    raise RuntimeError("Missing Desktop HTML converter")
declaration = "    void BrowserLoaded(const char *html, unsigned length);"
call = "    desktop_->BrowserLoaded((const char *)body,count);"
if declarations.count(declaration) != 1 or http.count(call) != 1:
    raise RuntimeError("Missing Browser text interface")

converter = match["converter"].replace("sizeof(browserContent_)", "Desktop::BrowserTextCapacity()")
converter = converter.replace("browserContent_", "html")
parser = "static unsigned ExtractBrowserText(char *html,unsigned length)\n{\n"
parser += converter + "    return out;\n}\n\n"
publish = """void Desktop::BrowserLoaded(const char *text,unsigned length)
{
    const unsigned size = length < sizeof(browserContent_)-1 ? length : sizeof(browserContent_)-1;
    memcpy(browserContent_,text,size);
    browserContent_[size]=0;
    browserLoading_=false;
    BrowserStatus("HTTP 200 OK - text extracted");
}"""
text = text[:match.start()] + publish + text[match.end():]
declarations = declarations.replace(declaration, """    static constexpr unsigned BrowserTextCapacity() { return sizeof(browserContent_); }
    void BrowserLoaded(const char *text, unsigned length);""")
http = http.replace("static bool Fetch(", parser + "static bool Fetch(", 1)
http = http.replace(call, """    auto *page = reinterpret_cast<char *>(body);
    desktop_->BrowserLoaded(page,ExtractBrowserText(page,count));""")

desktop.write_text(text, encoding="utf-8")
header.write_text(declarations, encoding="utf-8")
browser.write_text(http, encoding="utf-8")
