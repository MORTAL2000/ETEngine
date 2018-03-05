#include "stdafx.hpp"
#include "FontLoader.hpp"

#include "FileSystem/Entry.h"
#include "FileSystem/BinaryReader.hpp"
#include "TextureLoader.hpp"

#include <ft2build.h>
#include <freetype/freetype.h>
#include "TextureData.hpp"
#include "SpriteRenderer.hpp"
//#include FT_FREETYPE_H  

FontLoader::FontLoader()
{
}

FontLoader::~FontLoader()
{
}

SpriteFont* FontLoader::LoadContent(const std::string& assetFile)
{
	ivec2 logPos = Logger::GetCursorPosition();
	std::string loadingString = std::string("Loading Font: ") + assetFile + " . . .";

	LOG(loadingString + " . . . opening file          ", Info, false, logPos);

	File* input = new File( assetFile, nullptr );
	if(!input->Open( FILE_ACCESS_MODE::Read ))
	{
		LOG(loadingString + " . . . FAILED!          ", Warning, false, logPos);
		LOG("    Opening font file failed.", Warning);
		return nullptr;
	}
	std::vector<uint8> binaryContent = input->Read();
	std::string extension = input->GetExtension();
	delete input; 
	input = nullptr;
	if(binaryContent.size() == 0)
	{
		LOG(loadingString + " . . . FAILED!          ", Warning, false, logPos);
		LOG("    Font file is empty.", Warning);
		return nullptr;
	}

	SpriteFont* ret = nullptr;

	if (extension == "ttf")
	{
		LOG(loadingString + " . . . loading ttf          ", Info, false, logPos);
		ret = LoadTtf(binaryContent);
	}
	else if (extension == "fnt")
	{
		LOG(loadingString + " . . . loading fnt data          ", Info, false, logPos);
		ret = LoadFnt(binaryContent, assetFile);
	}
	else
	{
		LOG(loadingString + " . . . FAILED!         ", Warning, false, logPos);
		LOG("    Cannot load font with this extension. Supported exensions:", Warning);
		LOG("        ttf", Warning);
		LOG("        fnt", Warning);
		return nullptr;
	}

	if (!ret)
	{
		LOG(loadingString + " . . . FAILED!         ", Warning, false, logPos);
	}
	else
	{
		LOG(loadingString + " . . . SUCCESS!          ", Info, false, logPos);
	}
	return ret;
}

void FontLoader::Destroy(SpriteFont* objToDestroy)
{
	if (!(objToDestroy == nullptr))
	{
		delete objToDestroy;
		objToDestroy = nullptr;
	}
}

SpriteFont* FontLoader::LoadTtf(const std::vector<uint8>& binaryContent)
{
	FT_Library ft;
	if (FT_Init_FreeType(&ft))
		LOG("FREETYPE: Could not init FreeType Library", Warning);

	FT_Face face;
	if (FT_New_Memory_Face(ft, binaryContent.data(), (FT_Long)binaryContent.size(), 0, &face))
		LOG("FREETYPE: Failed to load font", Warning);

	FT_Set_Pixel_Sizes(face, 0, m_FontSize);

	SpriteFont* pFont = new SpriteFont();
	pFont->m_FontSize = (int16)m_FontSize;
	pFont->m_FontName = std::string(face->family_name) + " - " + face->style_name;
	pFont->m_CharacterCount = face->num_glyphs;

	struct TempChar
	{
		FontMetric metric;
		TextureData* texture;
	};

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	//Load individual characters
	std::map<char, TempChar> characters;
	for (char c = 0; c < SpriteFont::CHAR_COUNT; c++)
	{
		if (FT_Load_Char(face, c, FT_LOAD_RENDER))
		{
			LOG("FREETYPE: Failed to load glyph", Warning);
			continue;
		}

		TempChar character;

		character.texture = new TextureData(face->glyph->bitmap.width, face->glyph->bitmap.rows, GL_RED, GL_RED, GL_UNSIGNED_BYTE);
		character.texture->Build(face->glyph->bitmap.buffer);

		TextureParameters params;
		params.wrapS = GL_CLAMP_TO_EDGE;
		params.wrapT = GL_CLAMP_TO_EDGE;
		character.texture->SetParameters(params);

		character.metric.IsValid = true;
		character.metric.Character = c;
		character.metric.Width = (uint16)face->glyph->bitmap.width;
		character.metric.Height = (uint16)face->glyph->bitmap.rows;
		character.metric.OffsetX = (int16)face->glyph->bitmap_left;
		character.metric.OffsetY = (int16)face->glyph->bitmap_top;

		characters[c] = character;
	}

	FT_Done_Face(face);
	FT_Done_FreeType(ft);

	//Generate atlas coordinates
	ivec2 startPos[4] = { ivec2(0), ivec2(0), ivec2(0), ivec2(0) };
	ivec2 maxPos[4] = { ivec2(0), ivec2(0), ivec2(0), ivec2(0) };
	bool horizontal = false;//Direction this pass expands the map in (internal moves are !horizontal)
	uint32 posCount = 1;//internal move count in this pass
	uint32 curPos = 0;//internal move count
	uint32 channel = 0;//channel to add to
	for (auto& character : characters)
	{
		character.second.metric.Page = 0;
		character.second.metric.Channel = (uint8)channel;
		character.second.metric.TexCoord = etm::vecCast<float>(startPos[channel]);
		if (horizontal)
		{
			startPos[channel].y += character.second.metric.Height;
			maxPos[channel].x = std::max(maxPos[channel].x, startPos[channel].x + character.second.metric.Width);
		}
		else
		{
			startPos[channel].x += character.second.metric.Width;
			maxPos[channel].y = std::max(maxPos[channel].y, startPos[channel].y + character.second.metric.Height);
		}
		channel++;
		if (channel == 4)
		{
			channel = 0;
			curPos++;
			if(curPos == posCount)
			{
				curPos = 0;
				horizontal = !horizontal;
				if (horizontal)
				{
					for(uint8 cha = 0; cha < 4; ++cha)startPos[cha] = ivec2(maxPos[cha].x, 0);
				}
				else
				{
					for (uint8 cha = 0; cha < 4; ++cha)startPos[cha] = ivec2(0, maxPos[cha].y);
					posCount++;
				}
			}
		}
	}

	//Setup rendering
	pFont->m_TextureWidth = std::max(std::max(maxPos[0].x, maxPos[1].x), std::max(maxPos[2].x, maxPos[3].x));
	pFont->m_TextureHeight = std::max(std::max(maxPos[0].y, maxPos[1].y), std::max(maxPos[2].y, maxPos[3].y));
	
	pFont->m_pTexture = new TextureData(pFont->m_TextureWidth, pFont->m_TextureHeight, GL_RGBA, GL_RGBA, GL_FLOAT);
	pFont->m_pTexture->Build();
	TextureParameters params(false);
	params.wrapS = GL_CLAMP_TO_EDGE;
	params.wrapT = GL_CLAMP_TO_EDGE;
	pFont->m_pTexture->SetParameters(params);
	GLuint captureFBO, captureRBO;

	glGenFramebuffers(1, &captureFBO);
	glGenRenderbuffers(1, &captureRBO);

	STATE->BindFramebuffer(captureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);

	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, pFont->m_TextureWidth, pFont->m_TextureHeight);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pFont->m_pTexture->GetHandle(), 0);

	STATE->SetViewport(ivec2(0), ivec2(pFont->m_TextureWidth, pFont->m_TextureHeight));
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//Render to atlas
	for (auto& character : characters)
	{
		vec4 color;
		switch (character.second.metric.Channel)
		{
		case 0: color = vec4(1, 0, 0, 0); break;
		case 1: color = vec4(0, 1, 0, 0); break;
		case 2: color = vec4(0, 0, 1, 0); break;
		case 3: color = vec4(0, 0, 0, 1); break;
		}
		SpriteRenderer::GetInstance()->Draw(character.second.texture, character.second.metric.TexCoord, color, 
			vec2(0), vec2(1), 0, 0, SpriteScalingMode::TEXTURE_ABS);
	}

	STATE->SetBlendEnabled(true);
	STATE->SetBlendEquation(GL_FUNC_ADD);
	STATE->SetBlendFunction(GL_ONE, GL_ONE);
	SpriteRenderer::GetInstance()->Draw();

	//Cleanup
	STATE->SetBlendEnabled(false);

	for (auto& character : characters)
	{
		pFont->SetMetric(character.second.metric, character.first);
		delete character.second.texture;
		character.second.texture = nullptr;
	}

	STATE->BindFramebuffer(0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	STATE->SetViewport(ivec2(0), WINDOW.Dimensions);

	glDeleteRenderbuffers(1, &captureRBO);
	glDeleteFramebuffers(1, &captureFBO);

	return pFont;
}

SpriteFont* FontLoader::LoadFnt(const std::vector<uint8>& binaryContent, const std::string& assetFile)
{
	auto pBinReader = new BinaryReader(); //Prevent memory leaks
	pBinReader->Open(binaryContent);

	if (!pBinReader->Exists())
	{
		delete pBinReader;
		LOG("SpriteFont::Load > Failed to read the assetFile!", Warning);

		return nullptr;
	}
	bool valid = false;
	if (pBinReader->Read<char>() == 'B')
	{
		if (pBinReader->Read<char>() == 'M')
		{
			if (pBinReader->Read<char>() == 'F')
			{
				valid = true;
			}
		}
	}
	if (!valid) 
	{
		LOG("Font file header invalid!", Warning);
		return nullptr;
	}
	if (pBinReader->Read<char>() < 3)
	{
		LOG("Font version invalid!", Warning);
		return nullptr;
	}

	SpriteFont* pFont = new SpriteFont();

	//**********
	// BLOCK 0 *
	//**********
	pBinReader->Read<char>();
	auto Block0Size = pBinReader->Read<int32>();
	int32 pos = pBinReader->GetBufferPosition();
	pFont->m_FontSize = pBinReader->Read<int16>();
	pBinReader->SetBufferPosition(pos + 14);
	std::string fn;
	char cur = pBinReader->Read<char>();
	while (cur != '\0')
	{
		fn += cur;
		cur = pBinReader->Read<char>();
	}
	pFont->m_FontName = fn;
	pBinReader->SetBufferPosition(pos + Block0Size);
	//**********
	// BLOCK 1 *
	//**********
	pBinReader->Read<char>();
	auto Block1Size = pBinReader->Read<int32>();
	pos = pBinReader->GetBufferPosition();
	pBinReader->SetBufferPosition(pos + 4);
	pFont->m_TextureWidth = pBinReader->Read<uint16>();
	pFont->m_TextureHeight = pBinReader->Read<uint16>();
	auto pagecount = pBinReader->Read<uint16>();
	if (pagecount > 1) LOG("SpriteFont::Load > SpriteFont(.fnt): Only one texture per font allowed", Warning);
	pBinReader->SetBufferPosition(pos + Block1Size);
	//**********
	// BLOCK 2 *
	//**********
	pBinReader->Read<char>();
	auto Block2Size = pBinReader->Read<int32>();
	pos = pBinReader->GetBufferPosition();
	std::string pn;
	cur = pBinReader->Read<char>();
	while (cur != '\0')
	{
		pn += cur;
		cur = pBinReader->Read<char>();
	}
	if (pn.size() == 0) LOG("SpriteFont::Load > SpriteFont(.fnt): Invalid Font Sprite [Empty]", Warning);
	auto filepath = assetFile.substr(0, assetFile.rfind('/') + 1);

	TextureLoader* pTL = ContentManager::GetLoader<TextureLoader, TextureData>();
	pTL->ForceResolution(true);
	pFont->m_pTexture = ContentManager::Load<TextureData>(filepath + pn);
	pTL->ForceResolution(false);
	pBinReader->SetBufferPosition(pos + Block2Size);
	//**********
	// BLOCK 3 *
	//**********
	pBinReader->Read<char>();
	auto Block3Size = pBinReader->Read<int32>();
	pos = pBinReader->GetBufferPosition();
	auto numChars = Block3Size / 20;
	pFont->m_CharacterCount = numChars;
	for (int32 i = 0; i < numChars; i++)
	{
		auto posChar = pBinReader->GetBufferPosition();
		auto charId = (wchar_t)(pBinReader->Read<uint32>());
		if (!(pFont->IsCharValid(charId)))
		{
			LOG("SpriteFont::Load > SpriteFont(.fnt): Invalid Character", Warning);
			pBinReader->SetBufferPosition(posChar + 20);
		}
		else
		{
			auto metric = &(pFont->GetMetric(charId));
			metric->IsValid = true;
			metric->Character = charId;
			auto xPos = pBinReader->Read<uint16>();
			auto yPos = pBinReader->Read<uint16>();
			metric->Width = pBinReader->Read<uint16>();
			metric->Height = pBinReader->Read<uint16>();
			metric->OffsetX = pBinReader->Read<int16>();
			metric->OffsetY = pBinReader->Read<int16>();
			metric->AdvanceX = pBinReader->Read<int16>();
			metric->Page = pBinReader->Read<uint8>();
			auto chan = pBinReader->Read<uint8>();
			switch (chan)
			{
			case 1: metric->Channel = 2; break;
			case 2: metric->Channel = 1; break;
			case 4: metric->Channel = 0; break;
			case 8: metric->Channel = 3; break;
			default: metric->Channel = 4; break;
			}
			metric->TexCoord = vec2((float)xPos / (float)pFont->m_TextureWidth
				, (float)yPos / (float)pFont->m_TextureHeight);
			pBinReader->SetBufferPosition(posChar + 20);
		}
	}
	delete pBinReader;

	return pFont;
}
