#pragma hdrstop
#include "precompiled.h"

#include "RenderCommon.h"


constexpr int kMaxDebugTextModels = 512;
constexpr int kMaxDebugLineModels = 16384;

struct DebugTextModel
{
	TextBufferHandle handle;
	idRenderMatrix modelMatrix;
};


struct DebugLineModel
{
	idVec4		rgb;
	idVec3		start;
	idVec3		end;
	bool		depthTest;
	int			lifeTime;
};

class RenderDebug_local : public RenderDebug
{
public:
	RenderDebug_local();

	virtual ~RenderDebug_local() override;

	virtual void Init() override;
	virtual void Shutdown() override;

	virtual void SubmitForDrawing() override;
	virtual void DebugText(const char* text, const idVec3& origin, float scale, const idVec4& color, const idMat3& viewAxis, const int align = 1, const int lifetime = 0, bool depthTest = false) override;
	virtual void DebugLine(const idVec4& color, const idVec3& start, const idVec3& end, const int lifetime = 0, const bool depthTest = false) override;

private:

	idArray<DebugTextModel, kMaxDebugTextModels> debugText;
	int numActiveDebugText = 0;

	idArray<DebugLineModel, kMaxDebugLineModels> debugLine;
	int numActiveDebugLine = 0;

	float shaderParms[MAX_ENTITY_SHADER_PARMS];

	const idMaterial* whiteMaterial = nullptr;
};


RenderDebug_local::RenderDebug_local()
{
}

RenderDebug_local::~RenderDebug_local()
{
}

void RenderDebug_local::Init()
{
	for (int i = 0; i < MAX_ENTITY_SHADER_PARMS; i++)
	{
		shaderParms[i] = 1.0f;
	}

	whiteMaterial = declManager->FindMaterial("_white");
}

void RenderDebug_local::Shutdown()
{
}

extern idCVar r_debugLineWidth;

extern idCVar stereoRender_defaultGuiDepth;

void RenderDebug_local::SubmitForDrawing()
{
	// Each debug like will have 4 vertices.
	vertCacheHandle_t vertexHandle = vertexCache.AllocVertex(NULL, 4 * (numActiveDebugLine + 1));
	vertCacheHandle_t indexHandle = vertexCache.AllocIndex(NULL, 6 * (numActiveDebugLine + 1));

	idDrawVert* vertexPtr = (idDrawVert*)vertexCache.MappedVertexBuffer(vertexHandle);
	triIndex_t* indexPointer = (triIndex_t*)vertexCache.MappedIndexBuffer(indexHandle);

	int numVerts = 0;
	int numIndexes = 0;

	const float lineWidth = std::max(1.0f, (float)r_debugLineWidth.GetInteger());

	for (int i = 0; i < numActiveDebugLine; i++)
	{
		const auto& line = debugLine[i];

		// we need the view direction to project the minor axis of the quad
		// as the view changes
		idVec3	localView, localTarget;
		float	modelMatrix[16];
		R_AxisToModelMatrix(mat3_identity, line.start, modelMatrix);
		R_GlobalPointToLocal(modelMatrix, tr.viewDef->renderView.vieworg, localView);
		R_GlobalPointToLocal(modelMatrix, line.end, localTarget);

		idVec3	major = localTarget;
		idVec3	minor;

		idVec3	mid = 0.5f * localTarget;
		idVec3	dir = mid - localView;
		minor.Cross(major, dir);
		minor.Normalize();

		minor *= (lineWidth * 0.2f);

		vertexPtr[numVerts + 0].xyz = line.start - minor;
		vertexPtr[numVerts + 0].SetColor(PackColor(line.rgb));

		vertexPtr[numVerts + 1].xyz = line.end - minor;
		vertexPtr[numVerts + 1].SetColor(PackColor(line.rgb));

		vertexPtr[numVerts + 2].xyz = line.end + minor;
		vertexPtr[numVerts + 2].SetColor(PackColor(line.rgb));

		vertexPtr[numVerts + 3].xyz = line.start + minor;
		vertexPtr[numVerts + 3].SetColor(PackColor(line.rgb));

		indexPointer[numIndexes + 0] = numVerts + 3;
		indexPointer[numIndexes + 1] = numVerts + 0;
		indexPointer[numIndexes + 2] = numVerts + 2;
		indexPointer[numIndexes + 3] = numVerts + 2;
		indexPointer[numIndexes + 4] = numVerts + 0;
		indexPointer[numIndexes + 5] = numVerts + 1;

		numVerts += 4;
		numIndexes += 6;
	}

	// Create the view
	if (whiteMaterial)
	{
		viewEntity_t* space = (viewEntity_t*)R_ClearedFrameAlloc(sizeof(*space), FRAME_ALLOC_VIEW_ENTITY);
		memcpy(space->modelMatrix, reinterpret_cast<float*>(&mat4_identity), sizeof(space->modelMatrix));
		R_MatrixMultiply(space->modelMatrix, tr.viewDef->worldSpace.modelViewMatrix, space->modelViewMatrix);
		space->weaponDepthHack = false;
		space->isGuiSurface = true;

		// If this is an in-game gui, we need to be able to find the matrix again for head mounted
		// display bypass matrix fixup.
		if (true) // link as entity
		{
			space->next = tr.viewDef->viewEntitys;
			tr.viewDef->viewEntitys = space;
		}

		//---------------------------
		// make a tech5 renderMatrix
		//---------------------------
		idRenderMatrix viewMat;
		idRenderMatrix::Transpose(*(idRenderMatrix*)space->modelViewMatrix, viewMat);
		idRenderMatrix::Multiply(tr.viewDef->projectionRenderMatrix, viewMat, space->mvp);
		if (false)
		{
			idRenderMatrix::ApplyDepthHack(space->mvp);
		}

		// to allow 3D-TV effects in the menu system, we define surface flags to set
		// depth fractions between 0=screen and 1=infinity, which directly modulate the
		// screenSeparation parameter for an X offset.
		// The value is stored in the drawSurf sort value, which adjusts the matrix in the
		// backend.
		float defaultStereoDepth = stereoRender_defaultGuiDepth.GetFloat();	// default to at-screen

		// add the surfaces to this view
		const idMaterial* shader = whiteMaterial;
		drawSurf_t* drawSurf = (drawSurf_t*)R_FrameAlloc(sizeof(*drawSurf), FRAME_ALLOC_DRAW_SURFACE);

		drawSurf->numIndexes = numIndexes;
		drawSurf->ambientCache = vertexHandle;
		drawSurf->indexCache = indexHandle;
		drawSurf->shadowCache = 0;
		drawSurf->jointCache = 0;
		drawSurf->frontEndGeo = NULL;
		drawSurf->space = space;
		drawSurf->material = shader;
		drawSurf->extraGLState = GLS_DEPTHFUNC_ALWAYS;
		drawSurf->scissorRect = tr.viewDef->scissor;
		drawSurf->sort = shader->GetSort();
		drawSurf->renderZFail = 0;
		// process the shader expressions for conditionals / color / texcoords
		const float* constRegs = shader->ConstantRegisters();
		if (constRegs)
		{
			// shader only uses constant values
			drawSurf->shaderRegisters = constRegs;
		}
		else
		{
			float* regs = (float*)R_FrameAlloc(shader->GetNumRegisters() * sizeof(float), FRAME_ALLOC_SHADER_REGISTER);
			drawSurf->shaderRegisters = regs;
			shader->EvaluateRegisters(regs, shaderParms, tr.viewDef->renderView.shaderParms, tr.viewDef->renderView.time[1] * 0.001f, NULL);
		}

		R_LinkDrawSurfToView(drawSurf, tr.viewDef);
	}

	numActiveDebugLine = 0;

	// Debug text draw
	TextBufferManager* man = renderSystem->GetTextBufferManager();

	for (int i = 0; i < numActiveDebugText; i++)
	{
		tr.guiModel->Clear();
		man->submitTextBuffer(debugText[i].handle);
		man->destroyTextBuffer(debugText[i].handle);
		tr.guiModel->EmitToCurrentView((float*)&debugText[i].modelMatrix, false);
		tr.guiModel->Clear();
	}

	// Debug line draw
	numActiveDebugText = 0;

}

void RenderDebug_local::DebugText(const char* text, const idVec3& origin, float scale, const idVec4& color, const idMat3& viewAxis, const int align, const int lifetime, bool depthTest)
{
	auto man = renderSystem->GetTextBufferManager();
	auto textHandle = man->createTextBuffer(0, BufferType::Dynamic);
	man->setPenPosition(textHandle, 0, 0);
	man->appendText(textHandle, renderSystem->GetDefaultFontHandle(), text);
	man->setTextColor(textHandle, VectorUtil::Vec4ToColorInt(color));
	man->setGlState(textHandle, GLS_DEPTHFUNC_ALWAYS);
	man->deformSprite(textHandle, viewAxis);

	float guiModelMatrix[16];
	// Scale is adjusted per font right now. Will need to setting on a nicer font. Preferably monospace.
	idMat3 axis = mat3_identity * scale * 0.5f;
	R_AxisToModelMatrix(axis, origin, guiModelMatrix);

	debugText[numActiveDebugText] = { textHandle, *reinterpret_cast<idRenderMatrix*>(guiModelMatrix) };
	numActiveDebugText++;
}

void RenderDebug_local::DebugLine(const idVec4& color, const idVec3& start, const idVec3& end, const int lifetime, const bool depthTest)
{
	debugLine[numActiveDebugLine] = { color, start, end, depthTest, lifetime };
	++numActiveDebugLine;
}


//====
// Render debug singleton
//====
RenderDebug* RenderDebug::instance = nullptr;

RenderDebug::~RenderDebug()
{
	if (instance)
	{
		delete instance;
	}
}

RenderDebug& RenderDebug::Get()
{
	if (instance)
	{
		return *instance;
	}

	instance = new RenderDebug_local();
	return *instance;
}
