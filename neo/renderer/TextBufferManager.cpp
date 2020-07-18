#include "precompiled.h"

#include "TextBufferManager.h"

#include <stddef.h> // offsetof
#include <wchar.h>  // wcslen

#include "TextBufferManager.h"
#include "CubeAtlas.h"
#include "GuiModel.h"


#define MAX_BUFFERED_CHARACTERS (8192 - 5)

extern idGuiModel* tr_guiModel;

class TextBuffer
{
public:

	/// TextBuffer is bound to a fontManager for glyph retrieval
	/// @remark the ownership of the manager is not taken
	TextBuffer(FontManager* _fontManager);
	~TextBuffer();

	void setStyle(uint32_t _flags = STYLE_NORMAL)
	{
		m_styleFlags = _flags;
	}

	void setTextColor(uint32_t _rgba = 0x000000FF)
	{
		m_textColor = toABGR(_rgba);
	}

	void setBackgroundColor(uint32_t _rgba = 0x000000FF)
	{
		m_backgroundColor = toABGR(_rgba);
	}

	void setOverlineColor(uint32_t _rgba = 0x000000FF)
	{
		m_overlineColor = toABGR(_rgba);
	}

	void setUnderlineColor(uint32_t _rgba = 0x000000FF)
	{
		m_underlineColor = toABGR(_rgba);
	}

	void setStrikeThroughColor(uint32_t _rgba = 0x000000FF)
	{
		m_strikeThroughColor = toABGR(_rgba);
	}

	void setPenPosition(float _x, float _y)
	{
		m_penX = _x; m_penY = _y;
	}

	/// Append an ASCII/utf-8 string to the buffer using current pen
	/// position and color.
	void appendText(FontHandle _fontHandle, const idStr& aString);

	/// Append a whole face of the atlas cube, mostly used for debugging
	/// and visualizing atlas.
	void appendAtlasFace(uint16_t _faceIndex);

	/// Clear the text buffer and reset its state (pen/color)
	void clearTextBuffer();

	/// Get pointer to the vertex buffer to submit it to the graphic card.
	const uint8_t* getVertexBuffer()
	{
		return (uint8_t*)m_vertexBuffer;
	}

	/// Number of vertex in the vertex buffer.
	uint32_t getVertexCount() const
	{
		return m_vertexCount;
	}

	/// Size in bytes of a vertex.
	uint32_t getVertexSize() const
	{
		return sizeof(idDrawVert);
	}

	/// get a pointer to the index buffer to submit it to the graphic
	const uint16_t* getIndexBuffer() const
	{
		return m_indexBuffer;
	}

	/// number of index in the index buffer
	uint32_t getIndexCount() const
	{
		return m_indexCount;
	}

	/// Size in bytes of an index.
	uint32_t getIndexSize() const
	{
		return sizeof(triIndex_t);
	}

	uint32_t getTextColor() const
	{
		return toABGR(m_textColor);
	}

	TextRectangle getRectangle() const
	{
		return m_rectangle;
	}

	const idMaterial* getMaterial() const
	{
		return m_material;
	}

private:
	void appendGlyph(FontHandle _handle, CodePoint _codePoint);
	void verticalCenterLastLine(float _txtDecalY, float _top, float _bottom);

	static uint32_t toABGR(uint32_t _rgba)
	{
		return (((_rgba >> 0) & 0xff) << 24)
			| (((_rgba >> 8) & 0xff) << 16)
			| (((_rgba >> 16) & 0xff) << 8)
			| (((_rgba >> 24) & 0xff) << 0)
			;
	}

	uint32_t m_styleFlags;

	// color states
	uint32_t m_textColor;

	uint32_t m_backgroundColor;
	uint32_t m_overlineColor;
	uint32_t m_underlineColor;
	uint32_t m_strikeThroughColor;

	//position states
	float m_penX;
	float m_penY;

	float m_originX;
	float m_originY;

	float m_lineAscender;
	float m_lineDescender;
	float m_lineGap;

	TextRectangle m_rectangle;
	FontManager* m_fontManager;
	const idMaterial* m_material;

	void setVertex(uint32_t _i, float _x, float _y, uint32_t _rgba, uint8_t _style = STYLE_NORMAL)
	{
		m_styleBuffer[_i] = _style;
		m_vertexBuffer[_i].xyz[0] = _x;
		m_vertexBuffer[_i].xyz[1] = _y;
		m_vertexBuffer[_i].SetColor(_rgba);
	}

	idDrawVert* m_vertexBuffer;
	triIndex_t* m_indexBuffer;
	uint8_t* m_styleBuffer;

	uint32_t m_indexCount;
	uint32_t m_lineStartIndex;
	uint16_t m_vertexCount;
};

TextBuffer::TextBuffer(FontManager* _fontManager)
	: m_styleFlags(STYLE_NORMAL)
	, m_textColor(0xffffffff)
	, m_backgroundColor(0xffffffff)
	, m_overlineColor(0xffffffff)
	, m_underlineColor(0xffffffff)
	, m_strikeThroughColor(0xffffffff)
	, m_penX(0)
	, m_penY(0)
	, m_originX(0)
	, m_originY(0)
	, m_lineAscender(0)
	, m_lineDescender(0)
	, m_lineGap(0)
	, m_fontManager(_fontManager)
	, m_material(nullptr)
	, m_vertexBuffer(new idDrawVert[MAX_BUFFERED_CHARACTERS * 4])
	, m_indexBuffer(new triIndex_t[MAX_BUFFERED_CHARACTERS * 6])
	, m_styleBuffer(new uint8_t[MAX_BUFFERED_CHARACTERS * 4])
	, m_indexCount(0)
	, m_lineStartIndex(0)
	, m_vertexCount(0)
{
	m_rectangle.width = 0;
	m_rectangle.height = 0;
	m_material = declManager->FindMaterial("_fontAtlas");
}

TextBuffer::~TextBuffer()
{
	delete[] m_vertexBuffer;
	delete[] m_indexBuffer;
	delete[] m_styleBuffer;
}

void TextBuffer::appendText(FontHandle _fontHandle, const idStr& aString)
{
	if (m_vertexCount == 0)
	{
		m_originX = m_penX;
		m_originY = m_penY;
		m_lineDescender = 0;
		m_lineAscender = 0;
		m_lineGap = 0;
	}

	CodePoint codepoint = 0;
	uint32_t state = 0;

	for (int i = 0; i < aString.Length(); ++i)
	{
		int d = i;
		appendGlyph(_fontHandle, (CodePoint)aString.UTF8Char(d));
	}
}

void TextBuffer::appendAtlasFace(uint16_t _faceIndex)
{
	if (m_vertexCount / 4 >= MAX_BUFFERED_CHARACTERS)
	{
		return;
	}

	float x0 = m_penX;
	float y0 = m_penY;
	float x1 = x0 + (float)m_fontManager->getAtlas()->getTextureSize();
	float y1 = y0 + (float)m_fontManager->getAtlas()->getTextureSize();

	m_fontManager->getAtlas()->packFaceLayerUV(_faceIndex, &m_vertexBuffer[m_vertexCount]);

	//setVertex(m_vertexCount + 0, x0, y0, m_backgroundColor);
	//setVertex(m_vertexCount + 1, x1, y0, m_backgroundColor);
	//setVertex(m_vertexCount + 2, x1, y1, m_backgroundColor);
	//setVertex(m_vertexCount + 3, x0, y1, m_backgroundColor);

	setVertex(m_vertexCount + 0, x0, y0, m_backgroundColor);
	setVertex(m_vertexCount + 1, x1, y0, m_backgroundColor);
	setVertex(m_vertexCount + 2, x1, y1, m_backgroundColor);
	setVertex(m_vertexCount + 3, x0, y1, m_backgroundColor);

	m_indexBuffer[m_indexCount + 0] = m_vertexCount + 3;
	m_indexBuffer[m_indexCount + 1] = m_vertexCount + 0;
	m_indexBuffer[m_indexCount + 2] = m_vertexCount + 2;
	m_indexBuffer[m_indexCount + 3] = m_vertexCount + 2;
	m_indexBuffer[m_indexCount + 4] = m_vertexCount + 0;
	m_indexBuffer[m_indexCount + 5] = m_vertexCount + 1;
	m_vertexCount += 4;
	m_indexCount += 6;
}

void TextBuffer::clearTextBuffer()
{
	m_penX = 0;
	m_penY = 0;
	m_originX = 0;
	m_originY = 0;

	m_vertexCount = 0;
	m_indexCount = 0;
	m_lineStartIndex = 0;
	m_lineAscender = 0;
	m_lineDescender = 0;
	m_lineGap = 0;
	m_rectangle.width = 0;
	m_rectangle.height = 0;
}

static triIndex_t quadPicIndexes[6] = { 3, 0, 2, 2, 0, 1 };

void TextBuffer::appendGlyph(FontHandle _handle, CodePoint _codePoint)
{
	if (_codePoint == L'\t')
	{
		for (uint32_t ii = 0; ii < 4; ++ii)
		{
			appendGlyph(_handle, L' ');
		}
		return;
	}

	const GlyphInfo* glyph = m_fontManager->getGlyphInfo(_handle, _codePoint);

	if (!glyph)
	{
		common->Warning("Glyph not found (font handle %d, code point %d)", _handle.id, _codePoint);
	}

	if (NULL == glyph)
	{
		return;
	}

	if (m_vertexCount / 4 >= MAX_BUFFERED_CHARACTERS)
	{
		return;
	}

	const FontInfo& font = m_fontManager->getFontInfo(_handle);

	if (_codePoint == L'\n')
	{
		m_penX = m_originX;
		m_penY += m_lineGap + m_lineAscender - m_lineDescender;
		m_lineGap = font.lineGap;
		m_lineDescender = font.descender;
		m_lineAscender = font.ascender;
		m_lineStartIndex = m_vertexCount;
		return;
	}

	//is there a change of font size that require the text on the left to be centered again ?
	if (font.ascender > m_lineAscender
		|| (font.descender < m_lineDescender))
	{
		if (font.descender < m_lineDescender)
		{
			m_lineDescender = font.descender;
			m_lineGap = font.lineGap;
		}

		float txtDecals = (font.ascender - m_lineAscender);
		m_lineAscender = font.ascender;
		m_lineGap = font.lineGap;
		verticalCenterLastLine((txtDecals), (m_penY - m_lineAscender), (m_penY + m_lineAscender - m_lineDescender + m_lineGap));
	}

	float kerning = 0 * font.scale;
	m_penX += kerning;

	const GlyphInfo& blackGlyph = m_fontManager->getBlackGlyph();
	const Atlas* atlas = m_fontManager->getAtlas();

	if (m_styleFlags & STYLE_BACKGROUND
		&& m_backgroundColor & 0xff000000)
	{
		float x0 = (m_penX - kerning);
		float y0 = (m_penY);
		float x1 = ((float)x0 + (glyph->advance_x));
		float y1 = (m_penY + m_lineAscender - m_lineDescender + m_lineGap);

		atlas->packUV(blackGlyph.regionIndex, &m_vertexBuffer[m_vertexCount]);

		const uint16_t vertexCount = m_vertexCount;
		setVertex(vertexCount + 0, x0, y0, m_backgroundColor, STYLE_BACKGROUND);
		setVertex(vertexCount + 1, x0, y1, m_backgroundColor, STYLE_BACKGROUND);
		setVertex(vertexCount + 2, x1, y1, m_backgroundColor, STYLE_BACKGROUND);
		setVertex(vertexCount + 3, x1, y0, m_backgroundColor, STYLE_BACKGROUND);

		m_indexBuffer[m_indexCount + 0] = vertexCount + 3;
		m_indexBuffer[m_indexCount + 1] = vertexCount + 0;
		m_indexBuffer[m_indexCount + 2] = vertexCount + 2;
		m_indexBuffer[m_indexCount + 3] = vertexCount + 2;
		m_indexBuffer[m_indexCount + 4] = vertexCount + 0;
		m_indexBuffer[m_indexCount + 5] = vertexCount + 1;
		m_vertexCount += 4;
		m_indexCount += 6;
	}

	if (m_styleFlags & STYLE_UNDERLINE
		&& m_underlineColor & 0xFF000000)
	{
		float x0 = (m_penX - kerning);
		float y0 = (m_penY + m_lineAscender - m_lineDescender * 0.5f);
		float x1 = ((float)x0 + (glyph->advance_x));
		float y1 = y0 + font.underlineThickness;

		atlas->packUV(blackGlyph.regionIndex, &m_vertexBuffer[m_vertexCount]);

		setVertex(m_vertexCount + 0, x0, y0, m_underlineColor, STYLE_UNDERLINE);
		setVertex(m_vertexCount + 1, x0, y1, m_underlineColor, STYLE_UNDERLINE);
		setVertex(m_vertexCount + 2, x1, y1, m_underlineColor, STYLE_UNDERLINE);
		setVertex(m_vertexCount + 3, x1, y0, m_underlineColor, STYLE_UNDERLINE);

		m_indexBuffer[m_indexCount + 0] = m_vertexCount + 3;
		m_indexBuffer[m_indexCount + 1] = m_vertexCount + 0;
		m_indexBuffer[m_indexCount + 2] = m_vertexCount + 2;
		m_indexBuffer[m_indexCount + 3] = m_vertexCount + 2;
		m_indexBuffer[m_indexCount + 4] = m_vertexCount + 0;
		m_indexBuffer[m_indexCount + 5] = m_vertexCount + 1;
		m_vertexCount += 4;
		m_indexCount += 6;
	}

	if (m_styleFlags & STYLE_OVERLINE
		&& m_overlineColor & 0xFF000000)
	{
		float x0 = (m_penX - kerning);
		float y0 = (m_penY);
		float x1 = ((float)x0 + (glyph->advance_x));
		float y1 = y0 + font.underlineThickness;

		atlas->packUV(blackGlyph.regionIndex, &m_vertexBuffer[m_vertexCount]);

		setVertex(m_vertexCount + 0, x0, y0, m_overlineColor, STYLE_OVERLINE);
		setVertex(m_vertexCount + 1, x0, y1, m_overlineColor, STYLE_OVERLINE);
		setVertex(m_vertexCount + 2, x1, y1, m_overlineColor, STYLE_OVERLINE);
		setVertex(m_vertexCount + 3, x1, y0, m_overlineColor, STYLE_OVERLINE);

		m_indexBuffer[m_indexCount + 0] = m_vertexCount + 3;
		m_indexBuffer[m_indexCount + 1] = m_vertexCount + 0;
		m_indexBuffer[m_indexCount + 2] = m_vertexCount + 2;
		m_indexBuffer[m_indexCount + 3] = m_vertexCount + 2;
		m_indexBuffer[m_indexCount + 4] = m_vertexCount + 0;
		m_indexBuffer[m_indexCount + 5] = m_vertexCount + 1;
		m_vertexCount += 4;
		m_indexCount += 6;
	}

	if (m_styleFlags & STYLE_STRIKE_THROUGH
		&& m_strikeThroughColor & 0xFF000000)
	{
		float x0 = (m_penX - kerning);
		float y0 = (m_penY + 0.666667f * font.ascender);
		float x1 = ((float)x0 + (glyph->advance_x));
		float y1 = y0 + font.underlineThickness;

		atlas->packUV(blackGlyph.regionIndex, &m_vertexBuffer[m_vertexCount]);

		setVertex(m_vertexCount + 0, x0, y0, m_strikeThroughColor, STYLE_STRIKE_THROUGH);
		setVertex(m_vertexCount + 1, x0, y1, m_strikeThroughColor, STYLE_STRIKE_THROUGH);
		setVertex(m_vertexCount + 2, x1, y1, m_strikeThroughColor, STYLE_STRIKE_THROUGH);
		setVertex(m_vertexCount + 3, x1, y0, m_strikeThroughColor, STYLE_STRIKE_THROUGH);

		m_indexBuffer[m_indexCount + 0] = m_vertexCount + 3;
		m_indexBuffer[m_indexCount + 1] = m_vertexCount + 0;
		m_indexBuffer[m_indexCount + 2] = m_vertexCount + 2;
		m_indexBuffer[m_indexCount + 3] = m_vertexCount + 2;
		m_indexBuffer[m_indexCount + 4] = m_vertexCount + 0;
		m_indexBuffer[m_indexCount + 5] = m_vertexCount + 1;
		m_vertexCount += 4;
		m_indexCount += 6;
	}

	float x0 = m_penX + (glyph->offset_x);
	float y0 = (m_penY + m_lineAscender + (glyph->offset_y));
	float x1 = (x0 + glyph->width);
	float y1 = (y0 + glyph->height);

	atlas->packUV(glyph->regionIndex, &m_vertexBuffer[m_vertexCount]);

	setVertex(m_vertexCount + 0, x0, y0, m_textColor);
	setVertex(m_vertexCount + 1, x1, y0, m_textColor);
	setVertex(m_vertexCount + 2, x1, y1, m_textColor);
	setVertex(m_vertexCount + 3, x0, y1, m_textColor);

	m_indexBuffer[m_indexCount + 0] = m_vertexCount + 3;
	m_indexBuffer[m_indexCount + 1] = m_vertexCount + 0;
	m_indexBuffer[m_indexCount + 2] = m_vertexCount + 2;
	m_indexBuffer[m_indexCount + 3] = m_vertexCount + 2;
	m_indexBuffer[m_indexCount + 4] = m_vertexCount + 0;
	m_indexBuffer[m_indexCount + 5] = m_vertexCount + 1;
	m_vertexCount += 4;
	m_indexCount += 6;

	m_penX += glyph->advance_x;
	if (m_penX > m_rectangle.width)
	{
		m_rectangle.width = m_penX;
	}

	if ((m_penY + m_lineAscender - m_lineDescender + m_lineGap) > m_rectangle.height)
	{
		m_rectangle.height = (m_penY + m_lineAscender - m_lineDescender + m_lineGap);
	}
}

void TextBuffer::verticalCenterLastLine(float _dy, float _top, float _bottom)
{
	for (uint32_t ii = m_lineStartIndex; ii < m_vertexCount; ii += 4)
	{
		if (m_styleBuffer[ii] == STYLE_BACKGROUND)
		{
			m_vertexBuffer[ii + 0].xyz[1] = _top;
			m_vertexBuffer[ii + 1].xyz[1] = _bottom;
			m_vertexBuffer[ii + 2].xyz[1] = _bottom;
			m_vertexBuffer[ii + 3].xyz[1] = _top;
		}
		else
		{
			m_vertexBuffer[ii + 0].xyz[1] += _dy;
			m_vertexBuffer[ii + 1].xyz[1] += _dy;
			m_vertexBuffer[ii + 2].xyz[1] += _dy;
			m_vertexBuffer[ii + 3].xyz[1] += _dy;
		}
	}
}

TextBufferManager::TextBufferManager(FontManager* _fontManager)
	: m_fontManager(_fontManager)
	, m_fontMaterial(nullptr)
{
	m_textBuffers = new BufferCache[MAX_TEXT_BUFFER_COUNT];
	m_fontMaterial = declManager->FindMaterial("_fontAtlas");
	if (!m_fontMaterial)
	{
		common->Warning("Failed to load font atlas");
	}
}

TextBufferManager::~TextBufferManager()
{
	assert(m_textBufferHandles.getNumHandles() == 0 && "All the text buffers must be destroyed before destroying the manager");
	delete[] m_textBuffers;
}

TextBufferHandle TextBufferManager::createTextBuffer(uint32_t _type, BufferType::Enum _bufferType)
{
	uint16_t textIdx = m_textBufferHandles.alloc();
	BufferCache& bc = m_textBuffers[textIdx];

	bc.textBuffer = new TextBuffer(m_fontManager);
	bc.fontType = _type;
	bc.bufferType = _bufferType;
	bc.indexBufferHandle = 0;
	bc.vertexBufferHandle = 0;

	TextBufferHandle ret = { textIdx };
	return ret;
}

void TextBufferManager::destroyTextBuffer(TextBufferHandle _handle)
{
	assert(_handle._id != kInvalidHandle && "Invalid handle used");

	BufferCache& bc = m_textBuffers[_handle._id];
	m_textBufferHandles.free(_handle._id);
	delete bc.textBuffer;
	bc.textBuffer = nullptr;
}

void TextBufferManager::submitTextBuffer(TextBufferHandle _handle, int32_t _depth)
{
	assert(_handle._id != kInvalidHandle && "Invalid handle used");

	BufferCache& bc = m_textBuffers[_handle._id];

	uint32_t indexSize = bc.textBuffer->getIndexCount() * bc.textBuffer->getIndexSize();
	uint32_t vertexSize = bc.textBuffer->getVertexCount() * bc.textBuffer->getVertexSize();

	if (0 == indexSize || 0 == vertexSize)
	{
		return;
	}

	idDrawVert* verts = tr_guiModel->AllocTris(bc.textBuffer->getVertexCount(), bc.textBuffer->getIndexBuffer(), bc.textBuffer->getIndexCount(), bc.textBuffer->getMaterial(), 0, STEREO_DEPTH_TYPE_NONE);
	WriteDrawVerts16(verts, (idDrawVert*)bc.textBuffer->getVertexBuffer(), bc.textBuffer->getVertexCount());

	/*

	renderSystem->DrawStretchPic(
		bc.textBuffer->
		renderSystem->GetVirtualWidth() / 4.0f, renderSystem->GetVirtualHeight() / 4.0f, renderSystem->GetVirtualWidth() / 2.0f, renderSystem->GetVirtualHeight() / 2.0f,
		-1.0f, 1.0f, 1.0f, -1.0f,
		fontMaterial,
		0.0f);

	bgfx::setTexture(0, s_texColor, m_fontManager->getAtlas()->getTextureHandle());

	bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
	switch (bc.fontType)
	{
	case FONT_TYPE_ALPHA:
		program = m_basicProgram;
		bgfx::setState(0
			| BGFX_STATE_WRITE_RGB
			| BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
		);
		break;

	case FONT_TYPE_DISTANCE:
		program = m_distanceProgram;
		bgfx::setState(0
			| BGFX_STATE_WRITE_RGB
			| BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
		);
		break;

	case FONT_TYPE_DISTANCE_SUBPIXEL:
		program = m_distanceSubpixelProgram;
		bgfx::setState(0
			| BGFX_STATE_WRITE_RGB
			| BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_FACTOR, BGFX_STATE_BLEND_INV_SRC_COLOR)
			, bc.textBuffer->getTextColor()
		);
		break;
	}

	switch (bc.bufferType)
	{
	case BufferType::Static:
	{
		bgfx::IndexBufferHandle ibh;
		bgfx::VertexBufferHandle vbh;

		if (bgfx::kInvalidHandle == bc.vertexBufferHandleIdx)
		{
			ibh = bgfx::createIndexBuffer(
				bgfx::copy(bc.textBuffer->getIndexBuffer(), indexSize)
			);

			vbh = bgfx::createVertexBuffer(
				bgfx::copy(bc.textBuffer->getVertexBuffer(), vertexSize)
				, m_vertexLayout
			);

			bc.vertexBufferHandleIdx = vbh.idx;
			bc.indexBufferHandleIdx = ibh.idx;
		}
		else
		{
			vbh.idx = bc.vertexBufferHandleIdx;
			ibh.idx = bc.indexBufferHandleIdx;
		}

		bgfx::setVertexBuffer(0, vbh, 0, bc.textBuffer->getVertexCount());
		bgfx::setIndexBuffer(ibh, 0, bc.textBuffer->getIndexCount());
	}
	break;

	case BufferType::Dynamic:
	{
		bgfx::DynamicIndexBufferHandle ibh;
		bgfx::DynamicVertexBufferHandle vbh;

		if (bgfx::kInvalidHandle == bc.vertexBufferHandleIdx)
		{
			ibh = bgfx::createDynamicIndexBuffer(
				bgfx::copy(bc.textBuffer->getIndexBuffer(), indexSize)
			);

			vbh = bgfx::createDynamicVertexBuffer(
				bgfx::copy(bc.textBuffer->getVertexBuffer(), vertexSize)
				, m_vertexLayout
			);

			bc.indexBufferHandleIdx = ibh.idx;
			bc.vertexBufferHandleIdx = vbh.idx;
		}
		else
		{
			ibh.idx = bc.indexBufferHandleIdx;
			vbh.idx = bc.vertexBufferHandleIdx;

			bgfx::update(
				ibh
				, 0
				, bgfx::copy(bc.textBuffer->getIndexBuffer(), indexSize)
			);

			bgfx::update(
				vbh
				, 0
				, bgfx::copy(bc.textBuffer->getVertexBuffer(), vertexSize)
			);
		}

		bgfx::setVertexBuffer(0, vbh, 0, bc.textBuffer->getVertexCount());
		bgfx::setIndexBuffer(ibh, 0, bc.textBuffer->getIndexCount());
	}
	break;

	case BufferType::Transient:
	{
		bgfx::TransientIndexBuffer tib;
		bgfx::TransientVertexBuffer tvb;
		bgfx::allocTransientIndexBuffer(&tib, bc.textBuffer->getIndexCount());
		bgfx::allocTransientVertexBuffer(&tvb, bc.textBuffer->getVertexCount(), m_vertexLayout);
		bx::memCopy(tib.data, bc.textBuffer->getIndexBuffer(), indexSize);
		bx::memCopy(tvb.data, bc.textBuffer->getVertexBuffer(), vertexSize);
		bgfx::setVertexBuffer(0, &tvb, 0, bc.textBuffer->getVertexCount());
		bgfx::setIndexBuffer(&tib, 0, bc.textBuffer->getIndexCount());
	}
	break;
	}

	bgfx::submit(_id, program, _depth);
	*/
}

void TextBufferManager::setStyle(TextBufferHandle _handle, uint32_t _flags)
{
	assert(_handle._id != kInvalidHandle && "Invalid handle used");
	BufferCache& bc = m_textBuffers[_handle._id];
	bc.textBuffer->setStyle(_flags);
}

void TextBufferManager::setTextColor(TextBufferHandle _handle, uint32_t _rgba)
{
	assert(_handle._id != kInvalidHandle && "Invalid handle used");
	BufferCache& bc = m_textBuffers[_handle._id];
	bc.textBuffer->setTextColor(_rgba);
}

void TextBufferManager::setBackgroundColor(TextBufferHandle _handle, uint32_t _rgba)
{
	assert(_handle._id != kInvalidHandle && "Invalid handle used");
	BufferCache& bc = m_textBuffers[_handle._id];
	bc.textBuffer->setBackgroundColor(_rgba);
}

void TextBufferManager::setOverlineColor(TextBufferHandle _handle, uint32_t _rgba)
{
	assert(_handle._id != kInvalidHandle && "Invalid handle used");
	BufferCache& bc = m_textBuffers[_handle._id];
	bc.textBuffer->setOverlineColor(_rgba);
}

void TextBufferManager::setUnderlineColor(TextBufferHandle _handle, uint32_t _rgba)
{
	assert(_handle._id != kInvalidHandle && "Invalid handle used");
	BufferCache& bc = m_textBuffers[_handle._id];
	bc.textBuffer->setUnderlineColor(_rgba);
}

void TextBufferManager::setStrikeThroughColor(TextBufferHandle _handle, uint32_t _rgba)
{
	assert(_handle._id != kInvalidHandle && "Invalid handle used");
	BufferCache& bc = m_textBuffers[_handle._id];
	bc.textBuffer->setStrikeThroughColor(_rgba);
}

void TextBufferManager::setPenPosition(TextBufferHandle _handle, float _x, float _y)
{
	assert(_handle._id != kInvalidHandle && "Invalid handle used");
	BufferCache& bc = m_textBuffers[_handle._id];
	bc.textBuffer->setPenPosition(_x, _y);
}

void TextBufferManager::appendText(TextBufferHandle _handle, FontHandle _fontHandle, const idStr& aString)
{
	assert(_handle._id != kInvalidHandle && "Invalid handle used");
	BufferCache& bc = m_textBuffers[_handle._id];
	bc.textBuffer->appendText(_fontHandle, aString);
}

void TextBufferManager::appendAtlasFace(TextBufferHandle _handle, uint16_t _faceIndex)
{
	assert(_handle._id != kInvalidHandle && "Invalid handle used");
	BufferCache& bc = m_textBuffers[_handle._id];
	bc.textBuffer->appendAtlasFace(_faceIndex);
}

void TextBufferManager::clearTextBuffer(TextBufferHandle _handle)
{
	assert(_handle._id != kInvalidHandle && "Invalid handle used");
	BufferCache& bc = m_textBuffers[_handle._id];
	bc.textBuffer->clearTextBuffer();
}

TextRectangle TextBufferManager::getRectangle(TextBufferHandle _handle) const
{
	assert(_handle._id != kInvalidHandle && "Invalid handle used");
	BufferCache& bc = m_textBuffers[_handle._id];
	return bc.textBuffer->getRectangle();
}