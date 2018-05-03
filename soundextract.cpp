/*
soundextract
Copyright 2018 Jonathan wilson

Soundextract is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version. See the file COPYING for more details.
*/

/* WWISE ADPCM decode algorithm and related information taken from reformat.c (copyright for that is given below)

The MIT License (MIT)

Copyright (c) 2016 Victor Dmitriyev

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <commdlg.h>
#include <CommCtrl.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>
#include "resource.h"
#include "tinyxml2.h"
#include "wwriff.h"

typedef uint32_t UInt32;
typedef uint16_t UInt16;
typedef UInt32 MediaID;
typedef UInt32 Fourcc;
constexpr UInt32 BankHeaderChunkID = 'DHKB';
constexpr UInt32 BankDataIndexChunkID = 'XDID';
constexpr UInt32 BankDataChunkID = 'ATAD';
constexpr Fourcc RIFFChunkId = 'FFIR';
constexpr Fourcc WAVEChunkId = 'EVAW';
constexpr Fourcc fmtChunkId = ' tmf';
constexpr Fourcc dataChunkId = 'atad';

#pragma pack(push,1)
struct SubchunkHeader
{
	UInt32 dwTag;
	UInt32 dwChunkSize;
};

struct BankHeader
{
	UInt32 dwBankGeneratorVersion;
	UInt32 dwSoundBankID;
	UInt32 dwLanguageID;
	UInt16 bFeedbackInBank;
	UInt16 bDeviceAllocated;
	UInt32 dwProjectID;
};

struct MediaHeader
{
	MediaID id;
	UInt32 uOffset;
	UInt32 uSize;
};

struct ChunkHeader
{
	Fourcc ChunkId;
	UInt32 dwChunkSize;
};
struct WaveFormatEx
{
	UInt16 wFormatTag;
	UInt16 nChannels;
	UInt32 nSamplesPerSec;
	UInt32 nAvgBytesPerSec;
	UInt16 nBlockAlign;
	UInt16 wBitsPerSample;
	UInt16 cbSize;
};

struct WaveFormatExtensible : public WaveFormatEx
{
	UInt16 wSamplesPerBlock;
	UInt32 dwChannelMask;
};

struct VorbisHeaderBase
{
	UInt32 dwTotalPCMFrames;
};

struct VorbisLoopInfo
{
	UInt32 dwLoopStartPacketOffset;
	UInt32 dwLoopEndPacketOffset;
	UInt16 uLoopBeginExtra;
	UInt16 uLoopEndExtra;
};

struct VorbisInfo
{
	VorbisLoopInfo LoopInfo;
	UInt32 dwSeekTableSize;
	UInt32 dwVorbisDataOffset;
	UInt16 uMaxPacketSize;
	UInt16 uLastGranuleExtra;
	UInt32 dwDecodeAllocSize;
	UInt32 dwDecodeX64AllocSize;
	UInt32 uHashCodebook;
	unsigned __int8 uBlockSizes[2];
};
struct VorbisHeader : public VorbisHeaderBase, public VorbisInfo
{
};
#pragma pack(pop)

struct Sound
{
	std::string id;
	std::string name;
	bool streamed;
};

char *datachunk;
std::string path;
std::vector<MediaHeader> media;
std::vector<Sound> sounds;

void LoadBank(std::string fname)
{
	FILE *f = fopen(fname.c_str(), "rb");
	SubchunkHeader sc;
	fread(&sc, sizeof(sc), 1, f);
	if (sc.dwTag == BankHeaderChunkID)
	{
		BankHeader h;
		fread(&h, sizeof(h), 1, f);
		fseek(f, sc.dwChunkSize - sizeof(h), SEEK_CUR);
		while (fread(&sc, sizeof(sc), 1, f))
		{
			switch (sc.dwTag)
			{
			case BankDataIndexChunkID:
				for (unsigned int i = 0; i < sc.dwChunkSize / sizeof(MediaHeader); i++)
				{
					MediaHeader m;
					fread(&m, sizeof(m), 1, f);
					media.push_back(m);
				}
				break;
			case BankDataChunkID:
				datachunk = new char[sc.dwChunkSize];
				fread(datachunk, sizeof(datachunk[0]), sc.dwChunkSize, f);
				break;
			default:
				fseek(f, sc.dwChunkSize, SEEK_CUR);
				break;
			}
		}
	}
	fclose(f);
}

void ParseFiles(tinyxml2::XMLNode *xml, bool streamed)
{
	for (xml = xml->FirstChildElement("File");xml;xml = xml->NextSiblingElement("File"))
	{
		Sound sound;
		sound.streamed = streamed;
		sound.id = xml->ToElement()->Attribute("Id");
		xml = xml->FirstChildElement("ShortName");
		sound.name = xml->FirstChild()->ToText()->Value();
		sound.name.erase(sound.name.rfind('.'));
		xml = xml->Parent();
		sounds.push_back(sound);
	}
}

int revorb(const char *fname);
BOOL CALLBACK DlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM /* lParam */)
{
	switch (Message)
	{
	case WM_CLOSE:
		EndDialog(hwnd, 0);
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_OPEN:
		{
			char lBuf[MAX_PATH] = "";
			OPENFILENAME of;
			memset(&of, 0, sizeof(OPENFILENAME));
			of.lStructSize = sizeof(OPENFILENAME);
			of.hwndOwner = hwnd;
			of.hInstance = nullptr;
			of.lpstrFilter = "XML files\0*.xml\0\0";
			of.lpstrCustomFilter = nullptr;
			of.nMaxCustFilter = 0;
			of.nFilterIndex = 0;
			of.lpstrFile = lBuf;
			of.nMaxFile = MAX_PATH;
			of.lpstrFileTitle = nullptr;
			of.nMaxFileTitle = 0;
			of.lpstrInitialDir = nullptr;
			of.lpstrTitle = nullptr;
			of.Flags = OFN_FILEMUSTEXIST;
			of.nFileOffset = 0;
			of.nFileExtension = 0;
			of.lpstrDefExt = nullptr;
			of.lCustData = 0;
			of.lpfnHook = nullptr;
			of.lpTemplateName = nullptr;
			if (GetOpenFileName(&of))
			{
				path = lBuf;
				path.erase(path.rfind('\\'));
				std::string bank = lBuf;
				bank.erase(bank.rfind('.'));
				bank += ".bnk";
				tinyxml2::XMLDocument doc;
				tinyxml2::XMLNode *xml = &doc;
				doc.LoadFile(lBuf);
				xml = xml->FirstChildElement("SoundBanksInfo");
				if (!xml)
				{
					return false;
				}
				xml = xml->FirstChildElement("SoundBanks");
				if (!xml)
				{
					return false;
				}
				xml = xml->FirstChildElement("SoundBank");
				if (!xml)
				{
					return false;
				}
				bool found = false;
				xml = xml->FirstChildElement("ReferencedStreamedFiles");
				if (xml)
				{
					ParseFiles(xml, true);
					sounds.erase(std::remove_if(sounds.begin(), sounds.end(), [](const Sound & s)
					{
						std::string fname = path;
						fname += L'\\';
						fname += s.id;
						fname += ".wem";
						struct _stat buf;
						return _stat(fname.c_str(), &buf) == -1;
					}
					), sounds.end());
					found = true;
				}
				xml = xml->Parent();
				xml = xml->FirstChildElement("IncludedMemoryFiles");
				if (xml)
				{
					ParseFiles(xml, false);
					found = true;
				}
				std::sort(sounds.begin(), sounds.end(), [](Sound s1, Sound s2)
				{
					return s1.name < s2.name;
				});
				if (!found)
				{
					return false;
				}
				HWND list = GetDlgItem(hwnd, IDC_SOUNDS);
				LVCOLUMN column;
				column.mask = LVCF_TEXT | LVCF_FMT;
				column.pszText = const_cast<LPSTR>("Sound");
				column.fmt = LVCFMT_LEFT;
				ListView_InsertColumn(list, 0, &column);
				for (auto &x : sounds)
				{
					LVITEM item;
					memset(&item, 0, sizeof(item));
					item.mask = LVIF_TEXT;
					item.iItem = 0xFFF;
					item.pszText = const_cast<LPSTR>(x.name.c_str());
					ListView_InsertItem(list, &item);
				}
				ListView_SetColumnWidth(list, 0, LVSCW_AUTOSIZE);
				LoadBank(bank);
			}
		}
		case IDC_EXTRACT:
		{
			HWND list = GetDlgItem(hwnd, IDC_SOUNDS);
			int item = ListView_GetNextItem(list, -1, LVNI_SELECTED);
			if (item != -1)
			{
				char *outdata = nullptr;
				long size = 0;
				if (sounds[item].streamed)
				{
					std::string infname = path;
					infname += '\\';
					infname += sounds[item].id;
					infname += ".wem";
					FILE *infile = fopen(infname.c_str(), "rb");
					fseek(infile, 0, SEEK_END);
					size = ftell(infile);
					fseek(infile, 0, SEEK_SET);
					outdata = new char[size];
					fread(outdata, sizeof(outdata[0]), size, infile);
					fclose(infile);
				}
				else
				{
					MediaID id = stoul(sounds[item].id);
					for (unsigned int i = 0; i < media.size(); i++)
					{
						if (media[i].id == id)
						{
							size = media[i].uSize;
							outdata = new char[size];
							memcpy(outdata, &datachunk[media[i].uOffset], size);
							break;
						}
					}
				}
				char *ptr = outdata;
				if (*reinterpret_cast<Fourcc *>(ptr) != RIFFChunkId)
				{
					delete[] outdata;
					return true;
				}
				ptr += sizeof(Fourcc);
				ptr += sizeof(UInt32);
				if (*reinterpret_cast<Fourcc *>(ptr) != WAVEChunkId)
				{
					delete[] outdata;
					return true;
				}
				ptr += sizeof(Fourcc);
				ChunkHeader header = *reinterpret_cast<ChunkHeader *>(ptr);
				if (header.ChunkId != fmtChunkId)
				{
					delete[] outdata;
					return true;
				}
				ptr += sizeof(ChunkHeader);
				WaveFormatExtensible format = *reinterpret_cast<WaveFormatExtensible *>(ptr);
				ptr += sizeof(WaveFormatExtensible);
				std::string ext;
				if (format.wFormatTag == 2)
				{
					if (header.dwChunkSize != sizeof(WaveFormatExtensible))
					{
						delete[] outdata;
						return true;
					}
					ext = ".wav";
				}
				else if (format.wFormatTag == 0xFFFE)
				{
					if (header.dwChunkSize != sizeof(WaveFormatExtensible))
					{
						delete[] outdata;
						return true;
					}
					ext = ".wav";
				}
				else if (format.wFormatTag == 0xFFFF)
				{
					if (header.dwChunkSize != sizeof(WaveFormatExtensible) + sizeof(VorbisHeader))
					{
						delete[] outdata;
						return true;
					}
					ext = ".ogg";
				}
				std::string sfname = sounds[item].name;
				sfname += ext;
				char lBuf[MAX_PATH] = "";
				strcpy(lBuf, sfname.c_str());
				OPENFILENAME of;
				memset(&of, 0, sizeof(OPENFILENAME));
				of.lStructSize = sizeof(OPENFILENAME);
				of.hwndOwner = hwnd;
				of.hInstance = nullptr;
				if (ext == ".wav")
				{
					of.lpstrFilter = "WAV files\0*.wav\0\0";
				}
				else
				{
					of.lpstrFilter = "OGG files\0*.ogg\0\0";
				}
				of.lpstrCustomFilter = nullptr;
				of.nMaxCustFilter = 0;
				of.nFilterIndex = 0;
				of.lpstrFile = lBuf;
				of.nMaxFile = MAX_PATH;
				of.lpstrFileTitle = nullptr;
				of.nMaxFileTitle = 0;
				of.lpstrInitialDir = nullptr;
				of.lpstrTitle = nullptr;
				of.Flags = OFN_OVERWRITEPROMPT;
				of.nFileOffset = 0;
				of.nFileExtension = 0;
				of.lpstrDefExt = nullptr;
				of.lCustData = 0;
				of.lpfnHook = nullptr;
				of.lpTemplateName = nullptr;
				if (GetSaveFileName(&of))
				{
					if (format.wFormatTag == 2)
					{
						format.wFormatTag = 0x11;
						format.wSamplesPerBlock = (format.nBlockAlign - 4 * format.nChannels) * 8 / (format.wBitsPerSample * format.nChannels) + 1;
						char *datapos = nullptr;
						UInt32 datasize = 0;
						while (ptr < outdata + size)
						{
							header = *reinterpret_cast<ChunkHeader *>(ptr);
							ptr += sizeof(ChunkHeader);
							if (header.ChunkId == dataChunkId)
							{
								datapos = ptr;
								datasize = header.dwChunkSize;
							}
							ptr += header.dwChunkSize;
						}
						if (!datapos || !datasize)
						{
							delete[] outdata;
							return true;
						}
						FILE *outfile = fopen(lBuf, "wb");
						header.ChunkId = RIFFChunkId;
						header.dwChunkSize = sizeof(Fourcc) + sizeof(ChunkHeader) + sizeof(WaveFormatExtensible) + sizeof(ChunkHeader) + datasize;
						fwrite(&header, sizeof(header), 1, outfile);
						Fourcc fcc = WAVEChunkId;
						fwrite(&fcc, sizeof(fcc), 1, outfile);
						header.ChunkId = fmtChunkId;
						header.dwChunkSize = sizeof(WaveFormatExtensible);
						fwrite(&header, sizeof(header), 1, outfile);
						fwrite(&format, sizeof(format), 1, outfile);
						header.ChunkId = dataChunkId;
						header.dwChunkSize = datasize;
						fwrite(&header, sizeof(header), 1, outfile);
						if (format.nChannels > 1)
						{
							ptr = datapos;
							static uint8_t transformInData[BUFSIZ], transformOutData[BUFSIZ];
							uint8_t *transformIn = transformInData,
								*transformOut = transformOutData;
							size_t transformCount = BUFSIZ / format.nBlockAlign;
							if (format.nBlockAlign > BUFSIZ)
							{
								transformIn = new uint8_t[2 * format.nBlockAlign];
								transformOut = transformIn + format.nBlockAlign;
								transformCount = 1;
							}
							for (size_t blockCount = 0; blockCount * format.nBlockAlign < datasize;)
							{
								size_t sz = format.nBlockAlign * transformCount;
								if (datapos + datasize < ptr + sz)
								{
									sz = datapos + datasize - ptr;
								}
								memcpy(transformIn, ptr, sz);
								ptr += sz;
								size_t blockAmount = sz / format.nBlockAlign;
								blockCount += blockAmount;
								for (size_t block = 0; block < blockAmount; block++)
								{
									for (size_t n = 0; n < format.nBlockAlign / (format.nChannels * 4u); n++)
									{
										for (size_t s = 0; s < format.nChannels; s++)
										{
											reinterpret_cast<uint32_t *>(transformOut + block * format.nBlockAlign)[n * format.nChannels + s] = reinterpret_cast<uint32_t *>(transformIn + block * format.nBlockAlign)[s * format.nBlockAlign / (format.nChannels * 4) + n];
										}
									}
								}
								fwrite(transformOut, format.nBlockAlign, blockAmount, outfile);
							}
							if (format.nBlockAlign > BUFSIZ)
							{
								delete[] transformIn;
							}
						}
						else
						{
							fwrite(datapos, 1, datasize, outfile);
						}
						fclose(outfile);
						delete[] outdata;
						return true;
					}
					else if (format.wFormatTag == 0xFFFE)
					{
						format.wFormatTag = 0x1;
						char *datapos = nullptr;
						UInt32 datasize = 0;
						while (ptr < outdata + size)
						{
							header = *reinterpret_cast<ChunkHeader *>(ptr);
							ptr += sizeof(ChunkHeader);
							if (header.ChunkId == dataChunkId)
							{
								datapos = ptr;
								datasize = header.dwChunkSize;
							}
							ptr += header.dwChunkSize;
						}
						if (!datapos || !datasize)
						{
							delete[] outdata;
							return true;
						}
						FILE *outfile = fopen(lBuf, "wb");
						header.ChunkId = RIFFChunkId;
						header.dwChunkSize = sizeof(Fourcc) + sizeof(ChunkHeader) + sizeof(WaveFormatExtensible) + sizeof(ChunkHeader) + datasize;
						fwrite(&header, sizeof(header), 1, outfile);
						Fourcc fcc = WAVEChunkId;
						fwrite(&fcc, sizeof(fcc), 1, outfile);
						header.ChunkId = fmtChunkId;
						header.dwChunkSize = sizeof(WaveFormatExtensible);
						fwrite(&header, sizeof(header), 1, outfile);
						fwrite(&format, sizeof(format), 1, outfile);
						header.ChunkId = dataChunkId;
						header.dwChunkSize = datasize;
						fwrite(&header, sizeof(header), 1, outfile);
						fwrite(datapos, 1, datasize, outfile);
						fclose(outfile);
						delete[] outdata;
						return true;
					}
					else if (format.wFormatTag == 0xFFFF)
					{
						if (size && outdata)
						{
							char *fn = tmpnam(nullptr);
							FILE *outfile = fopen(fn, "wb");
							fwrite(outdata, sizeof(outdata[0]), size, outfile);
							fclose(outfile);
							{
								Wwise_RIFF_Vorbis ww(fn);
								ofstream out(lBuf, ios::binary);
								ww.generate_ogg(out);
							}
							_unlink(fn);
							revorb(lBuf);
							delete[] outdata;
							return true;
						}
					}
				}
			}
		}
		default:
			return false;
		}
	default:
		return false;
	}
	return true;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /* hPrevInstance */,
	LPSTR /* lpCmdLine */, int /* nCmdShow */)
{
	return DialogBox(hInstance, MAKEINTRESOURCE(IDD_MAIN), nullptr, DlgProc);
}
