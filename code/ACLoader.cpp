
/*
---------------------------------------------------------------------------
Open Asset Import Library (ASSIMP)
---------------------------------------------------------------------------

Copyright (c) 2006-2008, ASSIMP Development Team

All rights reserved.

Redistribution and use of this software in source and binary forms, 
with or without modification, are permitted provided that the following 
conditions are met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the above
  copyright notice, this list of conditions and the
  following disclaimer in the documentation and/or other
  materials provided with the distribution.

* Neither the name of the ASSIMP team, nor the names of its
  contributors may be used to endorse or promote products
  derived from this software without specific prior
  written permission of the ASSIMP Development Team.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
---------------------------------------------------------------------------
*/

/** @file Implementation of the AC3D importer class */

#include "AssimpPCH.h"

#ifndef AI_BUILD_NO_AC_IMPORTER

// internal headers
#include "ACLoader.h"
#include "ParsingUtils.h"
#include "fast_atof.h"


using namespace Assimp;


// ------------------------------------------------------------------------------------------------
// skip to the next token
#define AI_AC_SKIP_TO_NEXT_TOKEN() \
	if (!SkipSpaces(&buffer)) \
	{ \
		DefaultLogger::get()->error("AC3D: Unexpected EOF/EOL"); \
		continue; \
	} 

// ------------------------------------------------------------------------------------------------
// read a string (may be enclosed in double quotation marks). buffer must point to "
#define AI_AC_GET_STRING(out) \
	++buffer; \
	const char* sz = buffer; \
	while ('\"' != *buffer) \
	{ \
		if (IsLineEnd( *buffer )) \
		{ \
			DefaultLogger::get()->error("AC3D: Unexpected EOF/EOL in string"); \
			out = "ERROR"; \
			break; \
		} \
		++buffer; \
	} \
	if (IsLineEnd( *buffer ))continue; \
	out = std::string(sz,(unsigned int)(buffer-sz)); \
	++buffer;


// ------------------------------------------------------------------------------------------------
// read 1 to n floats prefixed with an optional predefined identifier 
#define AI_AC_CHECKED_LOAD_FLOAT_ARRAY(name,name_length,num,out) \
	AI_AC_SKIP_TO_NEXT_TOKEN(); \
	if (name_length) \
	{ \
		if (strncmp(buffer,name,name_length) || !IsSpace(buffer[name_length])) \
		{ \
			DefaultLogger::get()->error("AC3D: Unexpexted token. " name " was expected."); \
			continue; \
		} \
		buffer += name_length+1; \
	} \
	for (unsigned int i = 0; i < num;++i) \
	{ \
		AI_AC_SKIP_TO_NEXT_TOKEN(); \
		buffer = fast_atof_move(buffer,((float*)out)[i]); \
	}


// ------------------------------------------------------------------------------------------------
// Constructor to be privately used by Importer
AC3DImporter::AC3DImporter()
{
}

// ------------------------------------------------------------------------------------------------
// Destructor, private as well 
AC3DImporter::~AC3DImporter()
{
}

// ------------------------------------------------------------------------------------------------
// Returns whether the class can handle the format of the given file. 
bool AC3DImporter::CanRead( const std::string& pFile, IOSystem* pIOHandler) const
{
	// simple check of file extension is enough for the moment
	std::string::size_type pos = pFile.find_last_of('.');
	// no file extension - can't read
	if( pos == std::string::npos)return false;
	std::string extension = pFile.substr( pos);

	for( std::string::iterator it = extension.begin(); it != extension.end(); ++it)
		*it = tolower( *it);

	if( extension == ".ac" || extension == "ac")
		return true;

	return false;
}

// ------------------------------------------------------------------------------------------------
// Get a pointer to the next line from the file
bool AC3DImporter::GetNextLine( )
{
	SkipLine(&buffer); 
	return SkipSpaces(&buffer);
}


// ------------------------------------------------------------------------------------------------
// Parse an object section in an AC file
void AC3DImporter::LoadObjectSection(std::vector<Object>& objects)
{
	if (!TokenMatch(buffer,"OBJECT",6))
		return;

	++mNumMeshes;

	objects.push_back(Object());
	Object& obj = objects.back();

	while (GetNextLine())
	{
		if (TokenMatch(buffer,"kids",4))
		{
			SkipSpaces(&buffer);
			unsigned int num = strtol10(buffer,&buffer);
			GetNextLine();
			if (num)
			{
				// load the children of this object recursively
				obj.children.reserve(num);
				for (unsigned int i = 0; i < num; ++i)
					LoadObjectSection(obj.children);
			}
			return;
		}
		else if (TokenMatch(buffer,"name",4))
		{
			SkipSpaces(&buffer);
			AI_AC_GET_STRING(obj.name);
		}
		else if (TokenMatch(buffer,"texture",7))
		{
			SkipSpaces(&buffer);
			AI_AC_GET_STRING(obj.texture);
		}
		else if (TokenMatch(buffer,"texrep",6))
		{
			SkipSpaces(&buffer);
			AI_AC_CHECKED_LOAD_FLOAT_ARRAY("",0,2,&obj.texRepeat);
		}
		else if (TokenMatch(buffer,"rot",3))
		{
			SkipSpaces(&buffer);
			AI_AC_CHECKED_LOAD_FLOAT_ARRAY("",0,9,&obj.rotation);
		}
		else if (TokenMatch(buffer,"loc",3))
		{
			SkipSpaces(&buffer);
			AI_AC_CHECKED_LOAD_FLOAT_ARRAY("",0,3,&obj.translation);
		}
		else if (TokenMatch(buffer,"numvert",7))
		{
			SkipSpaces(&buffer);

			unsigned int t = strtol10(buffer,&buffer);
			obj.vertices.reserve(t);
			for (unsigned int i = 0; i < t;++i)
			{
				if (!GetNextLine())
				{
					DefaultLogger::get()->error("AC3D: Unexpected EOF: not all vertices have been parsed yet");
					break;
				}
				else if (!IsNumeric(*buffer))
				{
					DefaultLogger::get()->error("AC3D: Unexpected token: not all vertices have been parsed yet");
					--buffer; // make sure the line is processed a second time
					break;
				}
				obj.vertices.push_back(aiVector3D());
				aiVector3D& v = obj.vertices.back();
				AI_AC_CHECKED_LOAD_FLOAT_ARRAY("",0,3,&v.x);
				//std::swap(v.z,v.y);
				v.z *= -1.f;
			}
		}
		else if (TokenMatch(buffer,"numsurf",7))
		{
			SkipSpaces(&buffer);
			
			bool Q3DWorkAround = false;

			const unsigned int t = strtol10(buffer,&buffer);
			obj.surfaces.reserve(t);
			for (unsigned int i = 0; i < t;++i)
			{
				GetNextLine();
				if (!TokenMatch(buffer,"SURF",4))
				{
					// FIX: this can occur for some files - Quick 3D for 
					// example writes no surf chunks
					if (!Q3DWorkAround)
					{
						DefaultLogger::get()->warn("AC3D: SURF token was expected");
						DefaultLogger::get()->debug("Continuing with Quick3D Workaround enabled");
					}
					--buffer; // make sure the line is processed a second time
					// break; --- see fix notes above

					Q3DWorkAround = true;
				}
				SkipSpaces(&buffer);
				obj.surfaces.push_back(Surface());
				Surface& surf = obj.surfaces.back();
				surf.flags = strtol_cppstyle(buffer);
			
				while (1)
				{
					if(!GetNextLine())
					{
						DefaultLogger::get()->error("AC3D: Unexpected EOF: surface is incomplete");
						break;
					}
					if (TokenMatch(buffer,"mat",3))
					{
						SkipSpaces(&buffer);
						surf.mat = strtol10(buffer);
					}
					else if (TokenMatch(buffer,"refs",4))
					{
						// --- see fix notes above
						if (Q3DWorkAround)
						{
							if (!surf.entries.empty())
							{
								buffer -= 6;
								break;
							}
						}

						SkipSpaces(&buffer);
						const unsigned int m = strtol10(buffer);
						surf.entries.reserve(m);

						obj.numRefs += m;

						for (unsigned int k = 0; k < m; ++k)
						{
							if(!GetNextLine())
							{
								DefaultLogger::get()->error("AC3D: Unexpected EOF: surface references are incomplete");
								break;
							}
							surf.entries.push_back(Surface::SurfaceEntry());
							Surface::SurfaceEntry& entry = surf.entries.back();

							entry.first = strtol10(buffer,&buffer);
							SkipSpaces(&buffer);
							AI_AC_CHECKED_LOAD_FLOAT_ARRAY("",0,2,&entry.second);
						}
					}
					else 
					{

						--buffer; // make sure the line is processed a second time
						break;
					}
				}
			}
		}
	}
	DefaultLogger::get()->error("AC3D: Unexpected EOF: \'kids\' line was expected");
}

// ------------------------------------------------------------------------------------------------
// Convert a material from AC3DImporter::Material to aiMaterial
void AC3DImporter::ConvertMaterial(const Object& object,
	const Material& matSrc,
	MaterialHelper& matDest)
{
	aiString s;

	if (matSrc.name.length())
	{
		s.Set(matSrc.name);
		matDest.AddProperty(&s,AI_MATKEY_NAME);
	}
	if (object.texture.length())
	{
		s.Set(object.texture);
		matDest.AddProperty(&s,AI_MATKEY_TEXTURE_DIFFUSE(0));
	}

	matDest.AddProperty<aiColor3D>(&matSrc.rgb,1, AI_MATKEY_COLOR_DIFFUSE);
	matDest.AddProperty<aiColor3D>(&matSrc.amb,1, AI_MATKEY_COLOR_AMBIENT);
	matDest.AddProperty<aiColor3D>(&matSrc.emis,1,AI_MATKEY_COLOR_EMISSIVE);
	matDest.AddProperty<aiColor3D>(&matSrc.spec,1,AI_MATKEY_COLOR_SPECULAR);

	int n;
	if (matSrc.shin)
	{
		n = aiShadingMode_Phong;
		matDest.AddProperty<float>(&matSrc.shin,1,AI_MATKEY_SHININESS);
	}
	else n = aiShadingMode_Gouraud;
	matDest.AddProperty<int>(&n,1,AI_MATKEY_SHADING_MODEL);

	float f = 1.f - matSrc.trans;
	matDest.AddProperty<float>(&f,1,AI_MATKEY_OPACITY);
}

// ------------------------------------------------------------------------------------------------
// Converts the loaded data to the internal verbose representation
aiNode* AC3DImporter::ConvertObjectSection(Object& object,
	std::vector<aiMesh*>& meshes,
	std::vector<MaterialHelper*>& outMaterials,
	const std::vector<Material>& materials)
{
	aiNode* node = new aiNode();
	if (object.vertices.size())
	{
		if (!object.surfaces.size() || !object.numRefs)
		{
			/* " An object with 7 vertices (no surfaces, no materials defined). 
			     This is a good way of getting point data into AC3D. 
			     The Vertex->create convex-surface/object can be used on these
			     vertices to 'wrap' a 3d shape around them "
				 (http://www.opencity.info/html/ac3dfileformat.html)

				 therefore: if no surfaces are defined return point data only
			 */

			DefaultLogger::get()->info("AC3D: No surfaces defined in object definition, "
				"a point list is returned");

			meshes.push_back(new aiMesh());
			aiMesh* mesh = meshes.back();

			mesh->mNumFaces = mesh->mNumVertices = (unsigned int)object.vertices.size();
			aiFace* faces = mesh->mFaces = new aiFace[mesh->mNumFaces];
			aiVector3D* verts = mesh->mVertices = new aiVector3D[mesh->mNumVertices];

			for (unsigned int i = 0; i < mesh->mNumVertices;++i,++faces,++verts)
			{
				*verts = object.vertices[i];
				faces->mNumIndices = 1;
				faces->mIndices = new unsigned int[1];
				faces->mIndices[0] = i;
			}

			// use the primary material in this case. this should be the
			// default material if all objects of the file contain points
			// and no faces.
			mesh->mMaterialIndex = 0;
			outMaterials.push_back(new MaterialHelper());
			ConvertMaterial(object, materials[0], *outMaterials.back());
		}
		else
		{
			// need to generate one or more meshes for this object.
			// find out how many different materials we have
			typedef std::pair< unsigned int, unsigned int > IntPair;
			typedef std::vector< IntPair > MatTable;
			MatTable needMat(materials.size(),IntPair(0,0));

			std::vector<Surface>::iterator it,end = object.surfaces.end();
			std::vector<Surface::SurfaceEntry>::iterator it2,end2;

			for (it = object.surfaces.begin(); it != end; ++it)
			{
				register unsigned int idx = (*it).mat;
				if (idx >= needMat.size())
				{
					DefaultLogger::get()->error("AC3D: material index os out of range");
					idx = 0;
				}
				if ((*it).entries.empty())
				{
					DefaultLogger::get()->warn("AC3D: surface her zero vertex references");
				}

				// validate all vertex indices to make sure we won't crash here
				for (it2  = (*it).entries.begin(),
					 end2 = (*it).entries.end(); it2 != end2; ++it2)
				{
					if ((*it2).first >= object.vertices.size())
					{
						DefaultLogger::get()->warn("AC3D: Invalid vertex reference");
						(*it2).first = 0;
					}
				}

				if (!needMat[idx].first)++node->mNumMeshes;

				switch ((*it).flags & 0xf)
				{
					// closed line
				case 0x1:

					needMat[idx].first  += (unsigned int)(*it).entries.size();
					needMat[idx].second += (unsigned int)(*it).entries.size()<<1u;
					break;

					// unclosed line
				case 0x2:

					needMat[idx].first  += (unsigned int)(*it).entries.size()-1;
					needMat[idx].second += ((unsigned int)(*it).entries.size()-1)<<1u;
					break;

					// 0 == polygon, else unknown
				default:

					if ((*it).flags & 0xf)
					{
						DefaultLogger::get()->warn("AC3D: The type flag of a surface is unknown");
						(*it).flags &= ~(0xf);
					}

					// the number of faces increments by one, the number
					// of vertices by surface.numref.
					needMat[idx].first++;
					needMat[idx].second += (unsigned int)(*it).entries.size();
				};
			}
			unsigned int* pip = node->mMeshes = new unsigned int[node->mNumMeshes];
			unsigned int mat = 0;
			for (MatTable::const_iterator cit = needMat.begin(), cend = needMat.end();
				cit != cend; ++cit, ++mat)
			{
				if (!(*cit).first)continue;

				// allocate a new aiMesh object
				*pip++ = (unsigned int)meshes.size();
				aiMesh* mesh = new aiMesh();
				meshes.push_back(mesh);

				mesh->mMaterialIndex = (unsigned int)outMaterials.size();
				outMaterials.push_back(new MaterialHelper());
				ConvertMaterial(object, materials[mat], *outMaterials.back());

				// allocate storage for vertices and normals
				mesh->mNumFaces = (*cit).first;
				aiFace* faces = mesh->mFaces = new aiFace[mesh->mNumFaces];

				mesh->mNumVertices = (*cit).second;
				aiVector3D* vertices = mesh->mVertices = new aiVector3D[mesh->mNumVertices];
				unsigned int cur = 0;

				// allocate UV coordinates, but only if the texture name for the
				// surface is not empty
				aiVector3D* uv = NULL;
				if(object.texture.length())
				{
					uv = mesh->mTextureCoords[0] = new aiVector3D[mesh->mNumVertices];
					mesh->mNumUVComponents[0] = 2;
				}

				for (it = object.surfaces.begin(); it != end; ++it)
				{
					if (mat == (*it).mat)
					{
						const Surface& src = *it;

						// closed polygon
						unsigned int type = (*it).flags & 0xf; 
						if (!type)
						{
							aiFace& face = *faces++;
							if((face.mNumIndices = (unsigned int)src.entries.size()))
							{
								face.mIndices = new unsigned int[face.mNumIndices];
								for (unsigned int i = 0; i < face.mNumIndices;++i,++vertices)
								{
									const Surface::SurfaceEntry& entry = src.entries[i];
									face.mIndices[i] = cur++;

									// copy vertex positions
									*vertices = object.vertices[entry.first];

									// copy texture coordinates (apply the UV offset)
									if (uv)
									{
										uv->x =  entry.second.x * object.texRepeat.x;
										uv->y =  entry.second.y * object.texRepeat.y;
										++uv;
									}
								}
							}
						}
						else
						{
							
							it2  = (*it).entries.begin();

							// either a closed or an unclosed line
							register unsigned int tmp = (unsigned int)(*it).entries.size();
							if (0x2 == type)--tmp;
							for (unsigned int m = 0; m < tmp;++m)
							{
								aiFace& face = *faces++;

								face.mNumIndices = 2;
								face.mIndices = new unsigned int[2];
								face.mIndices[0] = cur++;
								face.mIndices[1] = cur++;

								// copy vertex positions
								*vertices++ = object.vertices[(*it2).first];
								
								// copy texture coordinates (apply the UV offset)
								if (uv)
								{
									uv->x =  (*it2).second.x * object.texRepeat.x;
									uv->y =  (*it2).second.y * object.texRepeat.y;
									++uv;
								}

								if (0x1 == type && tmp-1 == m)
								{
									// if this is a closed line repeat its beginning now
									it2  = (*it).entries.begin();
								}
								else ++it2;

								// second point
								*vertices++ = object.vertices[(*it2).first];

								if (uv)
								{
									uv->x =  (*it2).second.x * object.texRepeat.x;
									uv->y =  (*it2).second.y * object.texRepeat.y;
									++uv;
								}
							}
						}
					}
				}
			}
		}
	}

	// add children to the object
	if (object.children.size())
	{
		node->mNumChildren = (unsigned int)object.children.size();
		node->mChildren = new aiNode*[node->mNumChildren];
		for (unsigned int i = 0; i < node->mNumChildren;++i)
		{
			node->mChildren[i] = ConvertObjectSection(object.children[i],meshes,outMaterials,materials);
			node->mChildren[i]->mParent = node;
		}
	}

	node->mName.Set(object.name);

	// setup the local transformation matrix of the object
	node->mTransformation = aiMatrix4x4 ( object.rotation );

	node->mTransformation.a4 = object.translation.x;
	node->mTransformation.b4 = object.translation.y;
	node->mTransformation.c4 = object.translation.z;

	return node;
}

// ------------------------------------------------------------------------------------------------
void AC3DImporter::SetupProperties(const Importer* pImp)
{
	configSplitBFCull = pImp->GetPropertyInteger(AI_CONFIG_IMPORT_AC_SEPARATE_BFCULL,1) ? true : false;
}

// ------------------------------------------------------------------------------------------------
// Imports the given file into the given scene structure. 
void AC3DImporter::InternReadFile( const std::string& pFile, 
	aiScene* pScene, IOSystem* pIOHandler)
{
	boost::scoped_ptr<IOStream> file( pIOHandler->Open( pFile, "rb"));

	// Check whether we can read from the file
	if( file.get() == NULL)
		throw new ImportErrorException( "Failed to open AC3D file " + pFile + ".");

	const unsigned int fileSize = (unsigned int)file->FileSize();

	// allocate storage and copy the contents of the file to a memory buffer
	std::vector<char> mBuffer2(fileSize+1);
	file->Read(&mBuffer2[0], 1, fileSize);
	mBuffer2[fileSize] = '\0';
	buffer = &mBuffer2[0];
	mNumMeshes = 0;

	if (::strncmp(buffer,"AC3D",4))
		throw new ImportErrorException("AC3D: No valid AC3D file, magic sequence not found");

	// print the file format version to the console
	unsigned int version = HexDigitToDecimal( buffer[4] );
	char msg[3];
	itoa10(msg,3,version);
	DefaultLogger::get()->info(std::string("AC3D file format version: ") + msg);

	std::vector<Material> materials;
	materials.reserve(5);

	std::vector<Object> rootObjects;
	rootObjects.reserve(5);

	while (GetNextLine())
	{
		if (TokenMatch(buffer,"MATERIAL",8))
		{
			materials.push_back(Material());
			Material& mat = materials.back();

			// manually parse the material ... sscanf would use the buldin atof ...
			// Format: (name) rgb %f %f %f  amb %f %f %f  emis %f %f %f  spec %f %f %f  shi %d  trans %f

			AI_AC_SKIP_TO_NEXT_TOKEN();
			if ('\"' == *buffer)
			{
				AI_AC_GET_STRING(mat.name);
				AI_AC_SKIP_TO_NEXT_TOKEN();
			}

			AI_AC_CHECKED_LOAD_FLOAT_ARRAY("rgb",3,3,&mat.rgb);
			AI_AC_CHECKED_LOAD_FLOAT_ARRAY("amb",3,3,&mat.rgb);
			AI_AC_CHECKED_LOAD_FLOAT_ARRAY("emis",4,3,&mat.rgb);
			AI_AC_CHECKED_LOAD_FLOAT_ARRAY("spec",4,3,&mat.rgb);
			AI_AC_CHECKED_LOAD_FLOAT_ARRAY("shi",3,1,&mat.shin);
			AI_AC_CHECKED_LOAD_FLOAT_ARRAY("trans",5,1,&mat.trans);
		}
		LoadObjectSection(rootObjects);
	}

	if (rootObjects.empty() || !mNumMeshes)
	{
		throw new ImportErrorException("AC3D: No meshes have been loaded");
	}
	if (materials.empty())
	{
		DefaultLogger::get()->warn("AC3D: No material has been found");
		materials.push_back(Material());
	}

	mNumMeshes += (mNumMeshes>>2u) + 1;
	std::vector<aiMesh*> meshes;
	meshes.reserve(mNumMeshes);

	std::vector<MaterialHelper*> omaterials;
	materials.reserve(mNumMeshes);

	// generate a dummy root if there are multiple objects on the top layer
	Object* root;
	if (1 == rootObjects.size())
		root = &rootObjects[0];
	else
	{
		root = new Object();
	}

	// now convert the imported stuff to our output data structure
	pScene->mRootNode = ConvertObjectSection(*root,meshes,omaterials,materials);
	if (1 != rootObjects.size())delete root;

	// build output arrays
	if (meshes.empty())
	{
		throw new ImportErrorException("An unknown error occured during converting");
	}
	pScene->mNumMeshes = (unsigned int)meshes.size();
	pScene->mMeshes = new aiMesh*[pScene->mNumMeshes];
	::memcpy(pScene->mMeshes,&meshes[0],pScene->mNumMeshes*sizeof(void*));

	// build output arrays
	pScene->mNumMaterials = (unsigned int)omaterials.size();
	pScene->mMaterials = new aiMaterial*[pScene->mNumMaterials];
	::memcpy(pScene->mMaterials,&omaterials[0],pScene->mNumMaterials*sizeof(void*));
}

#endif //!defined AI_BUILD_NO_AC_IMPORTER