#ifndef BIOS_H
#define BIOS_H

extern void BIOS_ArcTan(void);
extern void BIOS_ArcTan2(void);
extern void BIOS_BitUnPack(void);
extern void BIOS_GetBiosChecksum(void);
extern void BIOS_BgAffineSet(void);
extern void BIOS_CpuSet(void);
extern void BIOS_CpuFastSet(void);
extern void BIOS_Diff8bitUnFilterWram(void);
extern void BIOS_Diff8bitUnFilterVram(void);
extern void BIOS_Diff16bitUnFilter(void);
extern void BIOS_Div(void);
extern void BIOS_DivARM(void);
extern void BIOS_HuffUnComp(void);
extern void BIOS_LZ77UnCompVram(void);
extern void BIOS_LZ77UnCompWram(void);
extern void BIOS_ObjAffineSet(void);
extern void BIOS_RLUnCompVram(void);
extern void BIOS_RLUnCompWram(void);
extern void BIOS_SoftReset(void);
extern void BIOS_Sqrt(void);
extern void BIOS_MidiKey2Freq(void);
extern void BIOS_SndDriverJmpTableCopy(void);

#endif // BIOS_H
