#include "stdafx.hpp"
#include "UIContainer.h"

#include <limits>
#include "SpriteRenderer.hpp"
#include "TextRenderer.hpp"

UIDynamicBox::~UIDynamicBox()
{
	for (auto child : m_RelativeChildren)delete child;
	for (auto child : m_DynamicChildren)delete child;
}

iRect UIDynamicBox::CalculateDimensions( const ivec2 &worldPos )
{
	m_WorldPos = worldPos;
	iRect ret = m_Rect;
	ret.pos = ret.pos + m_WorldPos;

	ivec2 minCorner = ivec2(std::numeric_limits<int32>::max());
	ivec2 maxCorner = ivec2(std::numeric_limits<int32>::lowest());
	for(auto child : m_RelativeChildren)
	{
		iRect childDim = child->CalculateDimensions( ret.pos );
		minCorner.x = min( minCorner.x, childDim.pos.x );
		minCorner.y = min( minCorner.y, childDim.pos.y );
		maxCorner.x = max( maxCorner.x, childDim.pos.x + childDim.size.x );
		maxCorner.y = max( maxCorner.y, childDim.pos.y + childDim.size.y );
	}

	ivec2 posOffset = ret.pos;
	m_Rect.size = ivec2( 0 );
	for(auto child : m_DynamicChildren)
	{
		ivec2 childSize = child->CalculateDimensions( posOffset ).size;
		switch(m_Mode)
		{
		case Mode::HORIZONTAL:
			posOffset.x += childSize.x;
			m_Rect.size.y = max(m_Rect.size.y, childSize.y);
			break;
		case Mode::VERTICAL:
			posOffset.y += childSize.y;
			m_Rect.size.x = max(m_Rect.size.x, childSize.x);
			break;
		}
	}

	switch(m_Mode)
	{
	case Mode::HORIZONTAL:
		m_Rect.size.x = posOffset.x - worldPos.x;
		break;
	case Mode::VERTICAL:
		m_Rect.size.y = posOffset.y - worldPos.y;
		break;
	}

	//Combine relative and dynamic for final size calculation
	minCorner.x = min( minCorner.x, m_Rect.pos.x );
	minCorner.y = min( minCorner.y, m_Rect.pos.y );
	maxCorner.x = max( maxCorner.x, m_Rect.pos.x + m_Rect.size.x );
	maxCorner.y = max( maxCorner.y, m_Rect.pos.y + m_Rect.size.y );
	m_Rect.pos = minCorner;
	m_Rect.size = maxCorner - minCorner;
	ret.pos = m_Rect.pos + m_WorldPos;
	ret.size = m_Rect.size;

	return ret;
}

bool UIDynamicBox::Draw( uint16 level ) 
{
	if (level <= m_Level)
	{
		if (etm::nearEqualsV(m_WorldPos, ivec2(0)))CalculateDimensions(ivec2(0));
		m_Level = level;
		return true;//has children, gotta go deeper
	}
	m_Level = level;
	bool ret = false;
	for(auto child : m_RelativeChildren)
	{
		ret |= child->Draw( level );
	}
	for(auto child : m_DynamicChildren)
	{
		ret |= child->Draw( level );
	}
	return ret;
}

void UIDynamicBox::Update()
{
	for (auto child : m_DynamicChildren)child->Update();
	for (auto child : m_RelativeChildren)child->Update();
}

void UIDynamicBox::AddChild( UIContainer* child, Positioning positioning )
{
	switch(positioning)
	{
	case Positioning::DYNAMIC:
		m_DynamicChildren.push_back( child );
		break;
	case Positioning::UI_RELATIVE:
		m_RelativeChildren.push_back( child );
		break;
	}
}

bool UIPortal::Draw(uint16 level)
{
	//Limit dimensions
	vec2 size = vec2((float)m_Rect.size.x, (float)m_Rect.size.y);
	vec2 pos = vec2((float)m_Rect.pos.x, (float)m_Rect.pos.y) + vec2((float)m_WorldPos.x, (float)m_WorldPos.y);

	ivec2 prevPos, prevSize;
	STATE->GetViewport(prevPos, prevSize);
	STATE->SetViewport(m_Rect.pos + m_WorldPos, m_Rect.size);

	//Render Background
	SpriteRenderer::GetInstance()->Draw(nullptr, pos, m_Color, vec2(0), size, 0, 1, SpriteScalingMode::SCREEN);
	SpriteRenderer::GetInstance()->Draw();

	//Render subcomponents
	if (m_Child)
	{
		bool draw = true;
		while (draw)
		{
			draw = m_Child->Draw(level);
			level++;
			SpriteRenderer::GetInstance()->Draw();
			TextRenderer::GetInstance()->Draw();
		}
	}
	//Restore viewport size
	STATE->SetViewport(prevPos, prevSize);
	return false;
}

iRect UIFixedContainer::CalculateDimensions(const ivec2 &worldPos)
{
	m_WorldPos = worldPos;
	iRect ret = m_Rect;
	ret.pos = ret.pos + m_WorldPos;
	return ret;
}

UISplitter::~UISplitter()
{
	delete m_First;
	delete m_Second;
}

bool UISplitter::Draw(uint16 level)
{
	bool ret = false;
	ret |= m_First->Draw(level);
	ret |= m_Second->Draw(level);
	return ret;
}

void UISplitter::Update()
{
	vec2 mousePos = INPUT->GetMousePosition();
	iRect overlapRegion;
	switch (m_Mode)
	{
	case UISplitter::Mode::HORIZONTAL:
		overlapRegion = iRect(
			ivec2((int32)((m_Rect.size.x*m_SplitPercentage) - m_SplitRegionPix), 0),
			ivec2((int32)m_SplitRegionPix * 2, m_Rect.size.y));
		break;
	case UISplitter::Mode::VERTICAL:
		overlapRegion = iRect(
			ivec2(0, (int32)((m_Rect.size.y*m_SplitPercentage) - m_SplitRegionPix)),
			ivec2(m_Rect.size.x, (int32)m_SplitRegionPix * 2));
		break;
	}
	if (overlapRegion.Contains(mousePos))
	{
		if (INPUT->IsMouseButtonPressed(SDL_BUTTON_LEFT))m_DragActive = true;
	}
	if (m_DragActive)
	{
		switch (m_Mode)
		{
		case UISplitter::Mode::HORIZONTAL:
			SetSplitPercentage((mousePos.x - (float)m_Rect.pos.x) / (float)m_Rect.size.x);
			break;
		case UISplitter::Mode::VERTICAL:
			SetSplitPercentage((mousePos.y - (float)m_Rect.pos.y) / (float)m_Rect.size.y);
			break;
		}
		if (INPUT->IsMouseButtonReleased(SDL_BUTTON_LEFT))
		{
			m_DragActive = false;
		}
	}
	m_First->Update();
	m_Second->Update();
}

void UISplitter::SetSize(ivec2 size)
{
	m_Rect.size = size;
	RecalculateSplit();
}

void UISplitter::SetSizeOnly(ivec2 size)
{
	m_Rect.size = size;
	RecalculateSplit(true);
}

void UISplitter::SetSplitPercentage(float perc)
{
	m_SplitPercentage = perc;
	RecalculateSplit(true);
}

void UISplitter::RecalculateSplit(bool sizeOnly)
{
	ivec2 firstSize;
	ivec2 secondSize;
	switch (m_Mode)
	{
	case UISplitter::Mode::HORIZONTAL:
		firstSize = ivec2((int32)(m_Rect.size.x*m_SplitPercentage), m_Rect.size.y);
		m_First->SetLocalPos(ivec2(0));
		secondSize = ivec2((int32)(m_Rect.size.x*(1 - m_SplitPercentage)), m_Rect.size.y);
		m_Second->SetLocalPos(ivec2((int32)(m_Rect.size.x*m_SplitPercentage), 0));
		break;
	case UISplitter::Mode::VERTICAL:
		firstSize = ivec2(m_Rect.size.x, (int32)(m_Rect.size.y*m_SplitPercentage));
		m_First->SetLocalPos(ivec2(0));
		secondSize = ivec2(m_Rect.size.x, (int32)(m_Rect.size.y*(1 - m_SplitPercentage)));
		m_Second->SetLocalPos(ivec2(0, (int32)(m_Rect.size.y*m_SplitPercentage)));
		break;
	}
	if (sizeOnly)
	{
		m_First->SetSizeOnly(firstSize);
		m_Second->SetSizeOnly(secondSize);
	}
	else
	{
		m_First->SetSize(firstSize);
		m_Second->SetSize(secondSize);
	}
}
