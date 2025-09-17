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
    "Usage: hddl -options\n"
    "HOMEd supported device list\n"
	"Version: 0.1\n"
	"Options:\n"
	"\th - help\n"
	"\td directory - base directory (default: /usr/share/homed-zigbee)\n"
	"\tf file - output file name (default: devs.md)\n";
}

void PutError() {
    Cerr() << "Invalid option(s)" << EOL;
    SetExitCode(3);
    PutHelp();
}

CONSOLE_APP_MAIN {
	// StdLogSetup(LOG_COUT|LOG_FILE);
    
    FileMapping fm;
    FileOut fo;
    FileIn fi;
    VectorMap<String, String> fnMap;
    VectorMap<String, Vector<json::Key>> bMap;
    // Default names
    String fon = "devs.md";
    String dn = "/usr/share/homed-zigbee";

    { // Handle command line arguments
        const Vector<String>& cmdline = CommandLine();
        for (int icount = cmdline.GetCount(), last = icount - 1, i = 0; i < icount; ++i) {
            const String& v = cmdline[i];
            if(*v != '-')
                return PutError();
            if (v.GetCount() > 1 && v[1] == '-') {
                NEVER();
            } else {
                for (const char *s = ~v + 1; *s; ++s) {
                    switch (*s) {
                    case 'h': return PutHelp();
                    case 'f':
                        if (i < last && cmdline[i + 1][0] != '-')
                            fon = cmdline[++i];
                        break;
                    case 'd':
                        if (i < last && cmdline[i + 1][0] != '-')
                            dn = cmdline[++i];
                        break;
                    default:
                        return PutError();
                    }
                }
            }
        }
    }

	if (!fo.Open(fon)) {
		Cerr() << "Couldn't create file " << fon;
		return;
	}

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

    { // Create header.
        fo << "# ZigBee: Поддерживаемые устройства";
        fo << EOL << EOL;
        fo << "## Общие сведения";
        fo << EOL << EOL;
        fo << "Список поддерживаемых устройств невелик, но он периодически пополняется. Для добавления поддержки новых устройств можно создать запрос на [GitHub](https://github.com/u236/homed-service-zigbee/issues) или заглянуть в [чат проекта](https://t.me/homed_chat) в Telegram.";
        fo << EOL << EOL;
        fo << "Представленный ниже список поддерживаемых устройств формируется из файлов библиотеки устройств, в полу-автоматическом режиме, поэтому он может быть не совсем актуальным.";
        fo << EOL << EOL;
    }

    // Collect data
    Vector<String> fv = FindAllPaths(dn, "*.json");
    for (const String& fin : fv) {
        // DUMP(GetFileName(fin));
        if (!fi.Open(fin)) {
            Cerr() << "Couldn't open file " << fin << EOL;
            continue;
        }
    
        const String fn = GetFileName(fin);
        const String content = LoadStream(fi);
        const Value js = json::Parse(content);
        if (js.IsError()) {
            Cerr() << "Failed to parse JSON file " << fin << EOL;
            continue;
        }

        if (js.GetCount() == 0) {
            Cerr() << "The JSON is empty. " << fin << EOL;
            continue;
        }

        Vector<json::Key>& vd = bMap.GetAdd(fn);
        if (js.Is<ValueMap>()) {
            const ValueMap vm = js;
            for (int icount = vm.GetCount(), i = 0; i < icount; ++i) {
                const ValueArray va = js[i];
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
        }
    }
	
    // Populate output file.
    for (int icount = fnMap.GetCount(), i = 0; i < icount; ++i) {
        const String& fn = fnMap.GetKey(i);
        const int ind = bMap.Find(fn);
        if (ind < 0)
            continue;
        fo << "## " << fnMap[i];
        fo << EOL << EOL;
        const Vector<json::Key>& kv = bMap[ind];
        for (const json::Key& k: kv) {
            const int line = k.GetLine();
            fo << "* [" << k.GetKey() << "](https://github.com/u236/homed-service-zigbee/blob/master/deploy/data/usr/share/homed-zigbee/" << fn << "#L" << line << ")";
            fo << EOL;
        }
        fo << EOL;
    }
}
