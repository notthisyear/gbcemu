#pragma once

#include <stdint.h>
#include <string>

namespace gbcemu {

class Cartridge {
  public:
    Cartridge(uint8_t *, uint64_t);

    void read_from_cartridge_switchable(uint8_t *, uint32_t, uint64_t) const;
    void read_from_cartridge_ram(uint8_t *, uint32_t, uint64_t) const;
    void write_to_cartridge_ram(uint8_t *, uint32_t, uint64_t);
    ~Cartridge();

  private:
    uint8_t *m_raw_data;
    uint64_t m_size;
    std::string m_title;

    struct HeaderOffset {
        const uint32_t TitleStart = 0x0134;
        const uint32_t TitleEnd = 0x0143;

        const uint32_t ManufacturerCodeStart = 0x013F;
        const uint32_t ManufacturerCodeEnd = 0x0142;

        const uint32_t CGBFlag = 0x0143;

        const uint32_t NewLicenseeCodeStart = 0x0144;
        const uint32_t NewLicenseeCodeEnd = 0x0145;

        const uint32_t SGBFlag = 0x0146;
        const uint32_t CartridgeType = 0x0147;
        const uint32_t ROMSize = 0x0148;
        const uint32_t RAMSize = 0x0149;

        const uint32_t DestinationCode = 0x014A;
        const uint32_t OldLicenseeCode = 0x014B;
        const uint32_t MaskROMVersionNumber = 0x014C;
        const uint32_t HeaderChecksum = 0x014D;

        const uint32_t GlobalChecksumStart = 0x014E;
        const uint32_t GlobalChecksumEnd = 0x014F;

        const uint8_t NewLicenseCodeFlag = 0x33;
    };

    const HeaderOffset m_header_offset;
};

// 00	None
// 01	Nintendo R&D1
// 08	Capcom
// 13	Electronic Arts
// 18	Hudson Soft
// 19	b-ai
// 20	kss
// 22	pow
// 24	PCM Complete
// 25	san-x
// 28	Kemco Japan
// 29	seta
// 30	Viacom
// 31	Nintendo
// 32	Bandai
// 33	Ocean/Acclaim
// 34	Konami
// 35	Hector
// 37	Taito
// 38	Hudson
// 39	Banpresto
// 41	Ubi Soft
// 42	Atlus
// 44	Malibu
// 46	angel
// 47	Bullet-Proof
// 49	irem
// 50	Absolute
// 51	Acclaim
// 52	Activision
// 53	American sammy
// 54	Konami
// 55	Hi tech entertainment
// 56	LJN
// 57	Matchbox
// 58	Mattel
// 59	Milton Bradley
// 60	Titus
// 61	Virgin
// 64	LucasArts
// 67	Ocean
// 69	Electronic Arts
// 70	Infogrames
// 71	Interplay
// 72	Broderbund
// 73	sculptured
// 75	sci
// 78	THQ
// 79	Accolade
// 80	misawa
// 83	lozc
// 86	Tokuma Shoten Intermedia
// 87	Tsukuda Original
// 91	Chunsoft
// 92	Video system
// 93	Ocean/Acclaim
// 95	Varie
// 96	Yonezawa/s’pal
// 97	Kaneko
// 99	Pack in soft
// A4	Konami (Yu-Gi-Oh!)

// 00- none               01- nintendo           08- capcom
// 09- hot-b              0A- jaleco             0B- coconuts
// 0C- elite systems      13- electronic arts    18- hudsonsoft
// 19- itc entertainment  1A- yanoman            1D- clary
// 1F- virgin             24- pcm complete       25- san-x
// 28- kotobuki systems   29- seta               30- infogrames
// 31- nintendo           32- bandai             33- "see above"
// 34- konami             35- hector             38- capcom
// 39- banpresto          3C- *entertainment i   3E- gremlin
// 41- ubi soft           42- atlus              44- malibu
// 46- angel              47- spectrum holoby    49- irem
// 4A- virgin             4D- malibu             4F- u.s. gold
// 50- absolute           51- acclaim            52- activision
// 53- american sammy     54- gametek            55- park place
// 56- ljn                57- matchbox           59- milton bradley
// 5A- mindscape          5B- romstar            5C- naxat soft
// 5D- tradewest          60- titus              61- virgin
// 67- ocean              69- electronic arts    6E- elite systems
// 6F- electro brain      70- infogrames         71- interplay
// 72- broderbund         73- sculptered soft    75- the sales curve
// 78- t*hq               79- accolade           7A- triffix entertainment
// 7C- microprose         7F- kemco              80- misawa entertainment
// 83- lozc               86- *tokuma shoten i   8B- bullet-proof software
// 8C- vic tokai          8E- ape                8F- i'max
// 91- chun soft          92- video system       93- tsuburava
// 95- varie              96- yonezawa/s'pal     97- kaneko
// 99- arc                9A- nihon bussan       9B- tecmo
// 9C- imagineer          9D- banpresto          9F- nova
// A1- hori electric      A2- bandai             A4- konami
// A6- kawada             A7- takara             A9- technos japan
// AA- broderbund         AC- toei animation     AD- toho
// AF- namco              B0- acclaim            B1- ascii or nexoft
// B2- bandai             B4- enix               B6- hal
// B7- snk                B9- pony canyon        BA- *culture brain o
// BB- sunsoft            BD- sony imagesoft     BF- sammy
// C0- taito              C2- kemco              C3- squaresoft
// C4- *tokuma shoten i   C5- data east          C6- tonkin house
// C8- koei               C9- ufl                CA- ultra
// CB- vap                CC- use                CD- meldac
// CE- *pony canyon or    CF- angel              D0- taito
// D1- sofel              D2- quest              D3- sigma enterprises
// D4- ask kodansha       D6- naxat soft         D7- copya systems
// D9- banpresto          DA- tomy               DB- ljn
// DD- ncs                DE- human              DF- altron
// E0- jaleco             E1- towachiki          E2- uutaka
// E3- varie              E5- epoch              E7- athena
// E8- asmik              E9- natsume            EA- king records
// EB- atlus              EC- epic/sony records  EE- igs
// F0- a wave         F3- extreme entertainment  FF- ljn

}