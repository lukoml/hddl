// vi set: noexpandtab
// License: BSD license
// Author: Sergey Sikorskiy
#include <Core/Core.h>

using namespace Upp;

namespace json {

    struct Key : Moveable<Key>, ValueType<Key, 10011> {
        Key(const Nuller&)                  { key = Null; line = Null; }
        Key(const String& key, int line) : key(key), line(line) {}
        Key() {}

        // We provide these methods to allow automatic conversion of Key to/from Value
        operator Value() const              { return RichToValue(*this); }
        Key(const Value& v)                 { *this = v.Get<Key>(); }

        String ToString() const             { return key; }
        unsigned GetHashValue() const       { return key.GetHashValue(); }
        void Serialize(Stream& s)           { s % key % line; }
        bool operator==(const Key& b) const { return key == b.key; }
        bool IsNullInstance() const         { return IsNull(key) && IsNull(line); }
        int  Compare(const Key& b) const    { return key.Compare(b.key); }
        // This type does not define XML nor Json serialization

        const String& GetKey() const { return key; }
        int GetLine() const { return line; }

    protected:
        String key;
        int line;
    };

    Value Parse(CParser& p) {
        p.UnicodeEscape();
        if(p.IsDouble())
            return p.ReadDouble();
        if(p.IsString()) {
            bool dt = p.IsChar2('\"', '\\');
            String s = p.ReadString();
            if(dt) {
                CParser p(s);
                if(p.Char('/') && p.Id("Date") && p.Char('(') && p.IsInt()) {
                    int64 n = p.ReadInt64();
                    if(!IsNull(n))
                        return Time(1970, 1, 1) + n / 1000;
                }
            }
            return s;
        }
        if(p.Id("null"))
            return Null;
        if(p.Id("true"))
            return true;
        if(p.Id("false"))
            return false;
        if(p.Char('{')) {
            ValueMap m;
            while(!p.Char('}')) {
                const int line = p.GetLine();
                const String key = p.ReadString();
                p.PassChar(':');
                m.Add(Key(key, line), Parse(p));
                if(p.Char('}')) // Stray ',' at the end of list is allowed...
                    break;
                p.PassChar(',');
            }
            return m;
        }
        if(p.Char('[')) {
            ValueArray va;
            while(!p.Char(']')) {
                va.Add(Parse(p));
                if(p.Char(']')) // Stray ',' at the end of list is allowed...
                    break;
                p.PassChar(',');
            }
            return va;
        }
        p.ThrowError("Unrecognized JSON element");
        return Null;
    }

    Value Parse(const char *s) {
        try {
            CParser p(s);
            return Parse(p);
        }
        catch(CParser::Error e) {
            return ErrorValue(e);
        }
    }

}

INITBLOCK {
	Value::Register<json::Key>();
}

void PutHelp() {
	Cout() << 
    "Usage: hddl [-options]\n"
    "HOMEd supported device list\n"
	"Version: 0.2\n"
	"Options:\n"
	"\th - help\n"
	"\td directory - base directory (default: GitHub website)\n"
	"\tf file - output file name (default: stdout)\n";
}

void PutErrorOpt() {
    Cerr() << "Invalid option(s)" << EOL;
    SetExitCode(3);
    PutHelp();
}

struct HOMEd {
    HOMEd();

    bool CollectDir(const String& dn);
    bool CollectGitHub();
    void Populate(Stream& os);

protected:
    bool CollectContent(const String& fn, const String& fin, const String& content);
    void Populate(Stream& os, const String& fn, const String& alias);

protected:
    VectorMap<String, String> fnMap;
    VectorMap<String, Vector<json::Key>> bMap;
}; // struct HOMEd

HOMEd::HOMEd() {
    { // Map file name to human readable name.
        fnMap.Add("lumi.json", "Aqara/Xiaomi");
        fnMap.Add("hue.json", "Philips");
        fnMap.Add("gledopto.json", "GLEDOPTO");
        fnMap.Add("gs.json", "GS");
        fnMap.Add("konke.json", "Konke");
        fnMap.Add("lifecontrol.json", "Life Control");
        fnMap.Add("orvibo.json", "ORVIBO");
        fnMap.Add("perenio.json", "Perenio");
        fnMap.Add("yandex.json", "Yandex");
        fnMap.Add("sonoff.json", "Sonoff");
        fnMap.Add("ikea.json", "IKEA");
        fnMap.Add("tuya.json", "TUYA");
        fnMap.Add("efekta.json", "Efekta");
        fnMap.Add("modkam.json", "Modkam");
        fnMap.Add("pushok.json", "PushOk");
        fnMap.Add("bacchus.json", "Bacchus");
        fnMap.Add("homed.json", "HOMEd");
        fnMap.Add("slacky.json", "Slacky");
        fnMap.Add("other.json", "...");
    }
}

bool HOMEd::CollectContent(const String& fn, const String& fin, const String& content) {
    // const bool known_fn = (fnMap.Find(fn) >= 0);
    const Value js = json::Parse(content);
    if (js.IsError()) {
        Cerr() << "Failed to parse JSON file " << fin << EOL;
        return false;
    }

    if (js.GetCount() == 0) {
        Cerr() << "The JSON is empty. " << fin << EOL;
        return false;
    }

    if (!js.Is<ValueMap>())
        return false;
    const ValueMap vm = js;
    // DUMP(vm);

    Vector<json::Key>& vd = bMap.GetAdd(fn);
    if (fnMap.Find(fn) < 0)
        // Register new section.
        fnMap.Add(fn, GetFileTitle(fn));
    for (int icount = vm.GetCount(), i = 0; i < icount; ++i) {
        // Vector<json::Key>& vd = bMap.GetAdd(known_fn ? fn : vm.GetKey(i).Get<json::Key>().GetKey());
        const ValueArray va = vm.GetValue(i);
        for (const Value& v : va) {
            if (v.Is<ValueMap>()) {
                const ValueMap obj = v;
                const int ind = obj.Find(json::Key("description", -1));
                if (ind >= 0) {
                    const json::Key& k = obj.GetKey(ind).Get<json::Key>();
                    const int line = k.GetLine();
                    const String& name = obj.GetValue(ind);
                    vd.Add(json::Key(name, line));
                }
            }
        }
    }
    return true;
}

bool HOMEd::CollectDir(const String& dn) {
    Vector<String> fv = FindAllPaths(dn, "*.json");
    for (const String& fin : fv) {
        FileIn fi;
        // DUMP(GetFileName(fin));
        if (!fi.Open(fin)) {
            Cerr() << "Couldn't open file " << fin << EOL;
            return false;
        }
    
        const String fn = GetFileName(fin);
        const String content = LoadStream(fi);
        if (!CollectContent(fn, fin, content))
            return false;
    }
    return true;
}

bool HOMEd::CollectGitHub() {
    const String api_url = "https://api.github.com/repos/u236/homed-service-zigbee/contents/deploy/data/usr/share/homed-zigbee";
	HttpRequest http(api_url);
	const String content = http.Method(HttpRequest::METHOD_GET).Execute();
	if (content.IsVoid()) {
		Cerr() << "Failed to execute GET request with error code " << http.GetStatusCode() << ". " << api_url << EOL;
		return false;
	}
    const Value js = ParseJSON(content);
    if (js.IsError()) {
        Cerr() << "Failed to parse JSON content. " << api_url << EOL;
		return false;
    }

    if (js.GetCount() == 0) {
        Cerr() << "The JSON is empty." << EOL;
		return false;
    }

    if (!js.Is<ValueArray>())
        return false;

    for (int icount = js.GetCount(), i = 0; i < icount; ++i) {
        const Value& v = js[i];
        if (v["type"] != "file")
            continue;
        // DUMP(v);
        const String fn = v["name"];
        if (GetFileExt(fn) != ".json")
            continue;
        const String download_url = v["download_url"];
        HttpRequest http(download_url);
        const String content = http.Method(HttpRequest::METHOD_GET).Execute();
        if (content.IsVoid()) {
            Cerr() << "Failed to execute GET request with error code " << http.GetStatusCode() << ". " << download_url << EOL;
            return false;
        }
        if (!CollectContent(fn, download_url, content))
            return false;
    }
    return true;
}

void HOMEd::Populate(Stream& os, const String& fn, const String& alias) {
    const int bi = bMap.Find(fn);
    os << "## " << alias;
    os << EOL << EOL;
    const Vector<json::Key>& kv = bMap[bi];
    for (const json::Key& k: kv) {
        const int line = k.GetLine();
        os << "* [" << k.GetKey() << "](https://github.com/u236/homed-service-zigbee/blob/master/deploy/data/usr/share/homed-zigbee/" << fn << "#L" << line << ")";
        os << EOL;
    }
    os << EOL;
}

void HOMEd::Populate(Stream& os) {
    { // Create header.
        os << "# ZigBee: Поддерживаемые устройства";
        os << EOL << EOL;
        os << "## Общие сведения";
        os << EOL << EOL;
        os << "Список поддерживаемых устройств невелик, но он периодически пополняется. Для добавления поддержки новых устройств можно создать запрос на [GitHub](https://github.com/u236/homed-service-zigbee/issues) или заглянуть в [чат проекта](https://t.me/homed_chat) в Telegram.";
        os << EOL << EOL;
        os << "Представленный ниже список поддерживаемых устройств формируется из файлов библиотеки устройств, в полу-автоматическом режиме, поэтому он может быть не совсем актуальным.";
        os << EOL << EOL;
    }

    SortByValue(fnMap);
    for (int icount = fnMap.GetCount(), i = 0; i < icount; ++i) {
        const String& fn = fnMap.GetKey(i);
        if (fn == "other.json")
            continue;
        Populate(os, fn, fnMap[i]);
    }

    // Handle other.json.
    Populate(os, "other.json", "...");

}

CONSOLE_APP_MAIN {
	// StdLogSetup(LOG_COUT|LOG_FILE);
    
    bool use_cout = true;
    bool use_github = true;
    FileOut fo;
    String fon;
    String dn;

    { // Handle command line arguments
        const Vector<String>& cmdline = CommandLine();
        for (int icount = cmdline.GetCount(), last = icount - 1, i = 0; i < icount; ++i) {
            const String& v = cmdline[i];
            if(*v != '-')
                return PutErrorOpt();
            if (v.GetCount() > 1 && v[1] == '-') {
                NEVER();
            } else {
                for (const char *s = ~v + 1; *s; ++s) {
                    switch (*s) {
                    case 'h': return PutHelp();
                    case 'f':
                        if (i < last && cmdline[i + 1][0] != '-')
                            fon = cmdline[++i];
                        use_cout = false;
                        break;
                    case 'd':
                        if (i < last && cmdline[i + 1][0] != '-')
                            dn = cmdline[++i];
                        use_github = false;
                        break;
                    default:
                        return PutErrorOpt();
                    }
                }
            }
        }
    }

	if (!use_cout && !fo.Open(fon)) {
		Cerr() << "Couldn't create file " << fon << EOL;
		return;
	}

    HOMEd hd;
    if (!(use_github ? hd.CollectGitHub() : hd.CollectDir(dn))) {
		Cerr() << "Couldn't collect data." << EOL;
		return;
    }
    hd.Populate(use_cout ? Cout() : fo);
}
