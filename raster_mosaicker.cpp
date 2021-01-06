#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include "gdal_priv.h"
#include "cpl_conv.h"
#include "gdalwarper.h"
#include "gdal_alg.h"
#include "cpl_string.h"
#include "raster_mosaicker.h"

constexpr auto GDAL_CACHE_MAX = 2147483647;

struct InitialTiff {
	std::string path_ndvi{};
	void* hTransformArg{};
	double adfDstGeoTransform[6]{};
	int nPixels{}, nLines{};
	GDALDataType dType{};
};

const char* outFormat = "GTiff";

int verbose;

void populateTiffs(std::vector<struct InitialTiff>& tiffArchive, const std::string& input_path, const std::string& input_filelist, const char* pszDstWKT) {
	std::ifstream input(input_path + "\\" + input_filelist, std::ios::binary | std::ios::in);
	std::string filename_iter;

	while (std::getline(input, filename_iter)) {
		struct InitialTiff additionalTiff;
		GDALDataset* poSrcDS;
		const char* pszSrcWKT;

		additionalTiff.path_ndvi = input_path + "\\" + filename_iter;

		std::cout << "TEST: " << additionalTiff.path_ndvi.c_str() << std::endl;

		if ((poSrcDS = (GDALDataset*)GDALOpen(additionalTiff.path_ndvi.c_str(), GA_ReadOnly)) != NULL) {
			pszSrcWKT = poSrcDS->GetProjectionRef();
			additionalTiff.hTransformArg = GDALCreateGenImgProjTransformer(poSrcDS, pszSrcWKT, NULL, pszDstWKT, FALSE, 0, 1);
			additionalTiff.dType = poSrcDS->GetRasterBand(1)->GetRasterDataType();
			GDALClose((GDALDatasetH)poSrcDS);

			if (verbose) {
				std::cout << "Successfully read " << filename_iter << std::endl;
			}
		}
		else {
			throw "Error opening input file.";
		}

		tiffArchive.push_back(additionalTiff);
	}
}

void addBounds(std::vector<struct InitialTiff>& tiffArchive) {
	CPLErr eErr;
	GDALDataset* poSrcDS;

	for (struct InitialTiff& currTiff : tiffArchive) {
		if ((poSrcDS = (GDALDataset*)GDALOpen(currTiff.path_ndvi.c_str(), GA_ReadOnly)) != NULL) {
			if ((eErr = GDALSuggestedWarpOutput(poSrcDS, GDALGenImgProjTransform, currTiff.hTransformArg,
				currTiff.adfDstGeoTransform, &currTiff.nPixels, &currTiff.nLines)) != CE_None) {
				throw "Cannot extract extent for tile!";
			}
		}

	}
	if (verbose) {
		std::cout << "Adding bounds successful!" << std::endl;
	}
}

int main(int argc, char** argv)
{
	GDALDataset* poDatasetSea;

	GDALRasterBand* poBand, * outBand;
	int nBlockXSize, nBlockYSize;
	int nXBlocks, nYBlocks;

	int pbSuccess;

	double adfGeoTransform[6];
	double mask_nodata, tile_nodata;
	const char* projRef;

	int empties = 0;
	int nonempties = 0;

	GDALDataset* poDstDS;
	char** papszOptions = NULL;
	GDALDriver* poDriver;
	GDALDataType maskType, inputType, outputType;

	GDALAllRegister();
	GDALSetCacheMax(GDAL_CACHE_MAX);

	std::vector<struct InitialTiff> tiffArchive{};
	std::string inputPath, seamask, ndviListFile, pszDstFilename;

	try {
		parseargs(argc, argv, inputPath, seamask, ndviListFile, pszDstFilename);
	}
	catch (const char* msg) {
		std::cerr << msg << std::endl;
		return EXIT_FAILURE;
	}

	if ((poDriver = GetGDALDriverManager()->GetDriverByName(outFormat)) == NULL) {
		std::cerr << "Cannot find driver " << outFormat << "!" << std::endl;
		return EXIT_FAILURE;
	}

	if ((poDatasetSea = (GDALDataset*)GDALOpen(seamask.c_str(), GA_ReadOnly)) != NULL) {
		if ((projRef = poDatasetSea->GetProjectionRef()) != NULL) {
			std::cout << "Projection Ref: " << projRef << std::endl;
		}

		if (poDatasetSea->GetGeoTransform(adfGeoTransform) == CE_None) {
			printf("Origin = (%.6f, %.6f)\n", adfGeoTransform[0], adfGeoTransform[3]);
			printf("Pixel size = (%.6f, %.6f)\n", adfGeoTransform[1], adfGeoTransform[5]);
		}

		/* use first band as the validity mask */
		poBand = poDatasetSea->GetRasterBand(1);

		poBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
		nXBlocks = (poBand->GetXSize() + nBlockXSize - 1) / nBlockXSize;
		nYBlocks = (poBand->GetYSize() + nBlockYSize - 1) / nBlockYSize;

		maskType = poBand->GetRasterDataType();
		if (maskType != GDT_Byte) {
			std::cerr << "Input must be a Byte raster!" << std::endl;
			return EXIT_FAILURE;
		}

		mask_nodata = poBand->GetNoDataValue(&pbSuccess);
		if (pbSuccess == 0) {
			std::cerr << "The target mask has no nodata value set. Assuming zero." << std::endl;
			mask_nodata = 0.0;
		}

		try {
			if (verbose) std::cout << "Populating vector of input tiles..." << std::endl;
			populateTiffs(tiffArchive, inputPath, ndviListFile, projRef);
			if (verbose) std::cout << "Finding out where each tile is located..." << std::endl;
			addBounds(tiffArchive);
		}
		catch (const char* msg) {
			std::cerr << msg << std::endl;
			return EXIT_FAILURE;
		}

		// find out input datatype
		outputType = tiffArchive[0].dType;

		// initialize destination dataset
		papszOptions = NULL;
		papszOptions = CSLSetNameValue(papszOptions, "TILED", "YES");
		papszOptions = CSLSetNameValue(papszOptions, "COMPRESS", "LZW");
		papszOptions = CSLSetNameValue(papszOptions, "BIGTIFF", "YES");
		papszOptions = CSLSetNameValue(papszOptions, "BLOCKXSIZE", std::to_string(nBlockXSize).c_str());
		papszOptions = CSLSetNameValue(papszOptions, "BLOCKYSIZE", std::to_string(nBlockYSize).c_str());
		papszOptions = CSLSetNameValue(papszOptions, "SPARSE_OK", "TRUE");

		poDstDS = poDriver->Create(pszDstFilename.c_str(), GDALGetRasterBandXSize(poBand), GDALGetRasterBandYSize(poBand), 1, outputType,
			papszOptions);
		poDstDS->SetGeoTransform(adfGeoTransform);
		poDstDS->SetProjection(projRef);
		outBand = poDstDS->GetRasterBand(1);
		outBand->SetNoDataValue(0.0);

		std::cout << "Raster has " << nXBlocks << " by " << nYBlocks << " blocks." << std::endl;

		for (int counter = 0; counter < nXBlocks * nYBlocks; counter++) {
			int i = counter % nXBlocks;
			int j = counter / nXBlocks;
			double left1, right1, top1, bottom1;
			double left2, right2, top2, bottom2;
			double dx, dy;
			unsigned char* rasterBlock;

			unsigned char* cOutBuf = NULL;
			float* fOutBuf = NULL;
			int nXValid, nYValid;
			GDALDataset* poSrcDS;
			GDALWarpOptions* psWarpOptions;
			GDALRasterBand* poSrcBand;
			double padfGeoTransform[6];
			int pbWasInitialized;
			void* pDataBuf = NULL;
			GDALWarpOperation oOperation;
			int bufi;
			int retval;

			poBand->GetActualBlockSize(i, j, &nXValid, &nYValid);
			retval = poBand->GetDataCoverageStatus(nBlockXSize * i, nBlockYSize * j, nXValid, nYValid, 0, NULL);

			if (retval == GDAL_DATA_COVERAGE_STATUS_EMPTY) {
				empties++;
			}
			else {
				if ((rasterBlock = (unsigned char*)CPLMalloc((size_t)nBlockXSize * nBlockYSize * sizeof(unsigned char))) == NULL) {
					fprintf(stderr, "Error in memory allocation! Exiting...\n");
					exit(1);
				}

				poBand->ReadBlock(i, j, rasterBlock);

				nonempties++;
				if (outputType == GDT_Byte) {
					if ((cOutBuf = (unsigned char*)CPLCalloc((size_t)nBlockXSize * nBlockYSize, sizeof(unsigned char))) == NULL) {
						fprintf(stderr, "Error in memory allocation! Exiting...\n");
						exit(1);
					}
				}
				else if (outputType == GDT_Float32) {
					if ((fOutBuf = (float*)CPLCalloc((size_t)nBlockXSize * nBlockYSize, sizeof(float))) == NULL) {
						fprintf(stderr, "Error in memory allocation! Exiting...\n");
						exit(1);
					}
				}
				else {
					std::cerr << "Unsupported datatype." << std::endl;
					return EXIT_FAILURE;
				}

				/* go through all and warp appropriate ones */
				for (auto const& currTiff : tiffArchive) {
					// candidate
					left1 = currTiff.adfDstGeoTransform[0];
					top1 = currTiff.adfDstGeoTransform[3];
					dx = currTiff.adfDstGeoTransform[1];
					dy = currTiff.adfDstGeoTransform[5];
					right1 = left1 + dx * currTiff.nPixels;
					bottom1 = top1 + dy * currTiff.nLines;

					// current block
					left2 = adfGeoTransform[0] + (double)nBlockXSize * i * adfGeoTransform[1];
					top2 = adfGeoTransform[3] + (double)nBlockYSize * j * adfGeoTransform[5];
					right2 = left2 + adfGeoTransform[1] * nXValid;
					bottom2 = top2 + adfGeoTransform[5] * nYValid;

					inputType = currTiff.dType;

					if (inputType != outputType) {
						std::cerr << "Incompatible input and output types!" << std::endl;
						return EXIT_FAILURE;
					}

					if (left1 <= right2 && left2 <= right1 && bottom1 <= top2 && bottom2 <= top1) {
						// warpable
						psWarpOptions = GDALCreateWarpOptions();
						if ((poSrcDS = (GDALDataset*)GDALOpen(currTiff.path_ndvi.c_str(), GA_ReadOnly)) == NULL) {
							std::cerr << "Cannot open " << currTiff.path_ndvi << ". Exit..." << std::endl;
							return EXIT_FAILURE;
						}
						poSrcBand = poSrcDS->GetRasterBand(1);
						tile_nodata = poSrcBand->GetNoDataValue(&pbSuccess);
						if (pbSuccess == 0) {
							std::cerr << "Input tile has no nodata value. Assuming zero." << std::endl;
							tile_nodata = 0.0;
						}
						psWarpOptions->hSrcDS = poSrcDS;
						psWarpOptions->hDstDS = NULL;
						psWarpOptions->nBandCount = 1;
						psWarpOptions->panSrcBands =
							(int*)CPLMalloc(sizeof(int) * psWarpOptions->nBandCount);
						psWarpOptions->panSrcBands[0] = 1;
						psWarpOptions->panDstBands =
							(int*)CPLMalloc(sizeof(int) * psWarpOptions->nBandCount);
						psWarpOptions->panDstBands[0] = 1;
						psWarpOptions->padfSrcNoDataReal = (double*)CPLMalloc(sizeof(double) * psWarpOptions->nBandCount);
						psWarpOptions->padfSrcNoDataReal[0] = tile_nodata;
						psWarpOptions->padfDstNoDataReal = (double*)CPLMalloc(sizeof(double) * psWarpOptions->nBandCount);
						psWarpOptions->padfDstNoDataReal[0] = tile_nodata;

						psWarpOptions->pfnProgress = GDALDummyProgress;
						psWarpOptions->eWorkingDataType = inputType;

						psWarpOptions->papszWarpOptions = CSLAddNameValue(psWarpOptions->papszWarpOptions, "INIT_DEST", "NO_DATA");
						// Establish reprojection transformer.
						psWarpOptions->pTransformerArg = currTiff.hTransformArg;
						psWarpOptions->pfnTransformer = GDALGenImgProjTransform;
						memcpy(padfGeoTransform, adfGeoTransform, sizeof(double) * 6);
						padfGeoTransform[0] = left2; // left
						padfGeoTransform[3] = top2; // top
						GDALSetGenImgProjTransformerDstGeoTransform(psWarpOptions->pTransformerArg, padfGeoTransform);

						// Initialize and execute the warp operation.

						oOperation.Initialize(psWarpOptions);
						if ((pDataBuf = oOperation.CreateDestinationBuffer(nBlockXSize, nBlockYSize, &pbWasInitialized)) == nullptr) {
							std::cerr << "Cannot initialize destination buffer." << std::endl;
							return EXIT_FAILURE;
						}

						if ((oOperation.WarpRegionToBuffer(0, 0, nBlockXSize, nBlockYSize, pDataBuf, inputType)) == CE_None) {
							// successfully warped
							// copy
							for (bufi = 0; bufi < nBlockXSize * nBlockYSize; bufi++) {
								if (inputType == GDT_Byte) {
									if (((double)*(rasterBlock + bufi) != mask_nodata) && ((double)*((unsigned char*)pDataBuf + bufi) != tile_nodata)) {
										*(cOutBuf + bufi) = *((unsigned char*)pDataBuf + bufi);
									}
								}
								else if (inputType == GDT_Float32) {
									if (((double)*(rasterBlock + bufi) != mask_nodata) && ((double)*((float*)pDataBuf + bufi) != tile_nodata)) {
										*(fOutBuf + bufi) = *((float*)pDataBuf + bufi);
									}
								}
								else {
									std::cerr << "Data type messed up?" << std::endl;
									return EXIT_FAILURE;
								}
							}
						}
						else {
							std::cerr << "Failure in warping tile!" << std::endl;
						}

						oOperation.DestroyDestinationBuffer(pDataBuf);
						GDALDestroyWarpOptions(psWarpOptions);

						GDALClose((GDALDatasetH)poSrcDS);
					}
					else {
						// doesn't overlap, ignore
					}

					void* pOutBuf;
					if (inputType == GDT_Byte) {
						pOutBuf = (void*)cOutBuf;
					}
					else if (inputType == GDT_Float32) {
						pOutBuf = (void*)fOutBuf;
					}
					else {
						pOutBuf = NULL;
					}

					if ((outBand->WriteBlock(i, j, pOutBuf)) == CE_Failure) {
						std::cerr << "Error writing block (" << i << ", " << j << "). Exit." << std::endl;
						return EXIT_FAILURE;
					}
					outBand->FlushCache();
				}

				CPLFree(rasterBlock);
				if (outputType == GDT_Byte) {
					CPLFree(cOutBuf);
				}
				else if (outputType == GDT_Float32) {
					CPLFree(fOutBuf);
				}
			}
		}

		std::cout << "Empties: " << empties;
		std::cout << "Nonempties: " << nonempties;
		GDALClose((GDALDatasetH)poDatasetSea);
		GDALClose((GDALDatasetH)poDstDS);
	}
	return EXIT_SUCCESS;
}
