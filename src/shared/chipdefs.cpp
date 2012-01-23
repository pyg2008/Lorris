#include <QString>
#include <QTextStream>
#include <QStringList>
#include <exception>
#include <stdexcept>
#include <QFile>
#include <vector>

#include "chipdefs.h"

// const std::string embedded_chipdefs, chipdefs.cpp
static const QString embedded_chipdefs =
    // lock bits, low fuses, high fuses, extended fuses
    "atmega48 avr:1e9205 flash=4096:64,eeprom=256:4 cksel:8,9,10,11 sut:12,13 ckout:14 ckdiv8:15 bodlevel:16,17,18 eesave:19"
        " wdton:20 spien:21:0 dwen:22 rstdisbl:23:1 selfprgen:24\n"
    "atmega88 avr:1e930a flash=8192:64,eeprom=512:4"
        " lb:0,1 blb0:2,3 blb1:4,5 cksel:8,9,10,11 sut:12,13 ckout:14 ckdiv8:15 bodlevel:16,17,18 eesave:19"
        " wdton:20 spien:21:0 dwen:22 rstdisbl:23:1 bootrst:24 bootsz:25,26\n"
    "atmega168 avr:1e9406 flash=16384:128,eeprom=512:4"
        " lb:0,1 blb0:2,3 blb1:4,5 cksel:8,9,10,11 sut:12,13 ckout:14 ckdiv8:15 bodlevel:16,17,18 eesave:19"
        " wdton:20 spien:21:0 dwen:22 rstdisbl:23:1 bootrst:24 bootsz:25,26\n"

    "atmega16 avr:1e9403 flash=16384:128,eeprom=512:4"
        " lb:0,1 blb0:2,3 blb1:4,5 cksel:8,9,10,11 sut:12,13 boden:14 bodlevel:15"
        " bootrst:16 bootsz:17,18 eesave:19 ckopt:20 spien:21:0 jtagen:22 ocden:23\n"

    "atmega128 avr:1e9702 flash=131072:256,eeprom=4096"
        " lb:0,1 blb0:2,3 blb1:4,5 cksel:8,9,10,11 sut:12,13 boden:14 bodlevel:15"
        " bootrst:16 bootsz:17,18 eesave:19 ckopt:20 spien:21:0 jtagen:22 ocden:23 wdton:24 m103c:25\n"

    // the datasheet rev D is wrong, the page size is 64w/128b
    "atmega8u2 avr:1e9389 flash=8192:128,eeprom=256:4"
        " lb:0,1 blb0:2,3 blb1:4,5"
        " cksel:8,9,10,11 sut:12,13 ckout:14 ckdiv8:15"
        " bootrst:16 bootsz:17,18 eesave:19 wdton:20 spien:21:0 rstdsbl:22:1 dwen:23:1"
        " bodlevel:24,25,26 hwbe:27\n"

    "atxmega128a avr:1e9746 flash=139264:512,eeprom=2048:32,fuses=8 jtaguid:0,1,2,3,4,5,6,7 wdper:8,9,10,11 wdwper:12,13,14,15 bodpd:16,17 bootrst:22 jtagen:32 wdlock:33 startuptime:34,35 rstdisbl:36:1"
        " bodlevel:40,41,42 eesave:43 bodact:44,45 lb:56,57 blbat:58,59 blba:60,61 blbb:62,63\n"
    "atxmega64a avr:1e9646 flash=69632:256,eeprom=2048:32,fuses=8 jtaguid:0,1,2,3,4,5,6,7 wdper:8,9,10,11 wdwper:12,13,14,15 bodpd:16,17 bootrst:22 jtagen:32 wdlock:33 startuptime:34,35 rstdisbl:36:1"
        " bodlevel:40,41,42 eesave:43 bodact:44,45 lb:56,57 blbat:58,59 blba:60,61 blbb:62,63\n"
    "atxmega32a avr:1e9541 flash=36864:256,eeprom=1024:32,fuses=8 jtaguid:0,1,2,3,4,5,6,7 wdper:8,9,10,11 wdwper:12,13,14,15 bodpd:16,17 bootrst:22 jtagen:32 wdlock:33 startuptime:34,35 rstdisbl:36:1"
        " bodlevel:40,41,42 eesave:43 bodact:44,45 lb:56,57 blbat:58,59 blba:60,61 blbb:62,63\n"
    "atxmega16a avr:1e9441 flash=20480:256,eeprom=1024:32,fuses=8 jtaguid:0,1,2,3,4,5,6,7 wdper:8,9,10,11 wdwper:12,13,14,15 bodpd:16,17 bootrst:22 jtagen:32 wdlock:33 startuptime:34,35 rstdisbl:36:1"
        " bodlevel:40,41,42 eesave:43 bodact:44,45 lb:56,57 blbat:58,59 blba:60,61 blbb:62,63\n"
    ;

void chip_definition::update_chipdef(std::vector<chip_definition> & templates, chip_definition & cd)
{
    for(quint32 i = 0; i < templates.size(); ++i)
    {
        chip_definition &templ = templates[i];
        if(cd.getSign() == templ.getSign())
        {
            cd.setName(templ.getName());
            cd.getMems().insert(templ.getMems().begin(), templ.getMems().end());

            for(quint32 x = 0; x < templ.getFuses().size(); ++x)
            {
                quint32 k;
                for(k = 0; k < cd.getFuses().size(); ++k)
                {
                    if(cd.getFuses()[k].name == templ.getFuses()[x].name)
                        break;
                }

                if(k == cd.getFuses().size())
                    cd.getFuses().push_back(templ.getFuses()[x]);
            }
        }
    }

    if(cd.getMems().find("fuses") == cd.getMems().end() && cd.getSign().left(4) == "avr:")
    {
        chip_definition::memorydef mem;
        mem.memid = 3;
        mem.size = 4;
        mem.pagesize = 0;
        cd.getMems()["fuses"] = mem;
    }
}

void chip_definition::parse_chipdefs(const QString &strdefs, std::vector<chip_definition> &res)
{
    QTextStream ss((QString*)&strdefs);
    QString line;

    for(line = ss.readLine(); !line.isNull(); line = ss.readLine())
    {
        QStringList tokens = line.split(' ', QString::SkipEmptyParts);

        if(tokens.size() < 3)
            continue;

        chip_definition def;
        def.setName(tokens[0]);
        def.setSign(tokens[1]);

        // parse memories
        QStringList memories = tokens[2].split(',', QString::SkipEmptyParts);
        for(quint8 i = 0; i < memories.size(); ++i)
        {
            QStringList memory_tokens = memories[i].split('=', QString::SkipEmptyParts);
            if(memory_tokens.size() != 2)
                continue;

            QStringList mem_size_tokens = memory_tokens[1].split(':', QString::SkipEmptyParts);

            chip_definition::memorydef memdef;
            memdef.memid = i + 1;
            memdef.size = mem_size_tokens[0].toInt();
            if(mem_size_tokens.size() > 1)
                memdef.pagesize = mem_size_tokens[1].toInt();
            else
                memdef.pagesize = 0;
            def.getMems()[memory_tokens[0]] = memdef;
        }

        // parse fuses
        for(quint32 i = 3; i < (quint32)tokens.size(); ++i)
        {
            if(tokens[i][0] == '!')
            {
                int sep_pos = tokens[i].indexOf('=');
                if(sep_pos == -1)
                    throw std::runtime_error("Invalid syntax in the chip definition file.");
                def.getOptions()[tokens[i].mid(1, sep_pos - 1)] = tokens[i].mid(sep_pos + 1);
            }
            else
            {
                QStringList token_parts = tokens[i].split(':', QString::SkipEmptyParts);
                if(token_parts.size() != 2 && token_parts.size() != 3)
                    continue;

                QStringList bit_numbers = token_parts[1].split(',', QString::SkipEmptyParts);

                chip_definition::fuse f;
                f.name = token_parts[0];
                for(quint32 j = 0; j < (quint32)bit_numbers.size(); ++j)
                    f.bits.push_back(bit_numbers[j].toInt());

                if(token_parts.size() == 3)
                {
                    bit_numbers = token_parts[2].split(',', QString::SkipEmptyParts);
                    for(quint32 j = 0; j < (quint32)bit_numbers.size(); ++j)
                        f.bits.push_back(bit_numbers[j].toInt());
                }
                def.getFuses().push_back(f);
            }
        }

        res.push_back(def);
    }
}

void chip_definition::parse_default_chipsets(std::vector<chip_definition> & res)
{
    chip_definition::parse_chipdefs(embedded_chipdefs, res);

    QFile *file = new QFile("./shupito_chipdefs.txt");
    if(!file->open(QIODevice::ReadOnly))
        return;

    QTextStream stream(file);
    QString defs("");
    QString line("");

    for(line = stream.readLine(); !line.isNull(); line = stream.readLine())
        if(line.length() != 0)
            defs += line + "\n";

    file->close();
    delete file;

    chip_definition::parse_chipdefs(defs, res);
}

chip_definition::chip_definition()
{
}

chip_definition::memorydef *chip_definition::getMemDef(quint8 memId)
{
    static const QString memNames[] = { "", "flash", "eeprom", "fuses" };
    return getMemDef(memNames[memId]);
}