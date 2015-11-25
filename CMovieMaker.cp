/*	A set of static functions for creating QTVR movies from a source PICT file.		Created 29 Jan 1996 by EGH		Copyright � 1996, Apple Computer, Inc.*/#include <ImageCompression.h>#include <String_Utils.h>#include "CBeachBall.h"#include "CUtils.h"#include "QTVRPanoAuthoring.h"#include "CMovieMaker.h"/* CMovieMaker::MakeAMovie	What we are here for. Takes the FSSpec for a source PICT file, a tile movie and	a destination QTVR movie file and creates them.		Return whether or not the operation completed. Note this function returns even	if an error occurs - exceptions are handled here.*/Boolean CMovieMaker::MakeAMovie(	Boolean inReplaceFiles,	const FSSpec &inSrcSpec,	const FSSpec &inTileSpec,	const FSSpec &inDestSpec,	Int16 inWidth,	Int16 inHeight,	Fixed inPan,	Fixed inTilt,	Fixed inZoom,	CodecType inCodec,	CodecQ inSpatialQuality,	Int16 inDepth,	ProgressProc inProgressProc){	Boolean completed = true;	Int16 errStrIndex;	PicHandle srcPicH = nil;	FSSpec		tileSpec = inTileSpec,		destSpec = inDestSpec;		try	{		if (inProgressProc("\pCreating tile movie"))			throw(Exception_UserCancelled);					// find unique file names if necessary		if (!inReplaceFiles)		{			FSSpec tempSpec; // a place for FSMakeFSSpec to do whatever it might			Int32 increment = 1; // just a number to place after the file name			unsigned char added = 0; // chars added to the name in case they need to be removed			OSErr result;			Str255				suffixStr,				numStr;						do			{				result = ::FSMakeFSSpec(tileSpec.vRefNum, tileSpec.parID, tileSpec.name,					&tempSpec);				if (result == noErr)				{						// the file exists, so try another name					tileSpec.name[0] -= added;										CopyPStr("\p_", suffixStr);					::NumToString(increment++, numStr);											// if necessary, chop off characters so the suffix will fit						// the changing part of the file name must be used					unsigned char charspace = 31 - suffixStr[0];					if (tileSpec.name[0] > charspace)						tileSpec.name[0] = charspace;										ConcatPStr(suffixStr, numStr);										added = suffixStr[0];					ConcatPStr(tileSpec.name, suffixStr);				}			} while (result == noErr);						added = 0;			increment = 1;						do			{				result = ::FSMakeFSSpec(destSpec.vRefNum, destSpec.parID, destSpec.name,					&tempSpec);				if (result == noErr)				{						// the file exists, so try another name					destSpec.name[0] -= added;					CopyPStr("\p_", suffixStr);					::NumToString(increment++, numStr);						// if necessary, chop off characters so the suffix will fit						// the changing part of the file name must be used					unsigned char charspace = 31 - suffixStr[0];					if (destSpec.name[0] > charspace)						destSpec.name[0] = charspace;										ConcatPStr(suffixStr, numStr);					added = suffixStr[0];					ConcatPStr(destSpec.name, suffixStr);				}			} while (result == noErr);		}					// open the pict file and read the picture into a handle		errStrIndex = err_OpenPicture;		LFile pictFile(inSrcSpec);		Int16 fileRef = pictFile.OpenDataFork(fsRdPerm);		Int32 logEOF;		ThrowIfOSErr_(::GetEOF(fileRef, &logEOF));		logEOF -= 512; // dont need the header		ThrowIfOSErr_(::SetFPos(fileRef, fsFromStart, 512));		srcPicH = (PicHandle)::NewHandle(logEOF);		ThrowIfMemError_();		ThrowIfOSErr_(::FSRead(fileRef, &logEOF, *srcPicH));		pictFile.CloseDataFork();					// examine the PICT's rectangle		Int16			picHeight = (*srcPicH)->picFrame.bottom - (*srcPicH)->picFrame.top,			picWidth = (*srcPicH)->picFrame.right - (*srcPicH)->picFrame.left;					// check for correct orientation		if (picHeight < picWidth)		{			CBeachBall::StopSpinningTask();			::SetCursor(&qd.arrow);			Str255				cwstr, chstr;			::NumToString(picWidth, cwstr);			::NumToString(picHeight, chstr);						gApp->BugUserTilSwitchedIn();			::ParamText(cwstr, chstr, "\p", "\p");			if (::CautionAlert(ALRT_ConfirmOrientation, nil) != ok)			{				errStrIndex = err_BadPictHeight;				throw(Exception_UserCancelled);			}						CBeachBall::StartSpinningTask(5);		}					// check for % 96 height and % 4 width		if ((picHeight % 96) != 0 || (picWidth % 4) != 0)		{			CBeachBall::StopSpinningTask();			::SetCursor(&qd.arrow);			Str255				hstr, wstr;						if ((picHeight % 96) != 0)			{				Int16 newPicHeight = picHeight - (picHeight % 96);				double scale = (double)newPicHeight / (double)picHeight;				Int16 newPicWidth = (Int16)((double)picWidth * scale);				newPicWidth = newPicWidth - (newPicWidth % 4);				::NumToString(newPicHeight, hstr);				::NumToString(newPicWidth, wstr);			}			else				::NumToString(picHeight, hstr);						if ((picWidth % 4) != 0)			{				Int16 newPicWidth = picWidth - (picWidth % 4);				::NumToString(newPicWidth, wstr);			}						Str255				cwstr, chstr;			::NumToString(picWidth, cwstr);			::NumToString(picHeight, chstr);						gApp->BugUserTilSwitchedIn();			::ParamText(cwstr, chstr, wstr, hstr);			if (::CautionAlert(ALRT_ConfirmSize, nil) != ok)			{				errStrIndex = err_BadPictHeight;				throw(Exception_UserCancelled);			}						CBeachBall::StartSpinningTask(5);		}					// create the tile movie file		errStrIndex = err_CreateTileMovie;		CreateTileMovie(srcPicH, tileSpec, inCodec, inSpatialQuality, inDepth, inProgressProc);		::DisposeHandle((Handle)srcPicH);		srcPicH = nil;					// create the single node movie		errStrIndex = err_CreatePanoMovie;		if (inProgressProc("\pCreating panorama movie"))			throw(Exception_UserCancelled);		CreateSingleNodeMovie(tileSpec, destSpec, inWidth, inHeight, inPan, inTilt, inZoom);	}	catch (ExceptionCode err)	{		if (srcPicH != nil)			::DisposeHandle((Handle)srcPicH);				CBeachBall::StopSpinningTask();				if (err != Exception_UserCancelled)			ReportError(err, errStrIndex);				completed = false;	}		return completed;}/* CMovieMaker::QTProgress	A progress function passed to QuickTime and called during	compression. This was implemented to provide the user	finer control over the UI during processing.		It simply calls the app's progress proc and returns an error if the	user aborted the operation.*///staticpascal OSErr CMovieMaker::QTProgress(	short inMessage,	Fixed inCompleteness,	long inRefcon){	if (((ProgressProc)inRefcon)(nil))		return codecAbortErr;	return noErr;}/* CMovieMaker::CreateSingleNodeMovie	Create a QuickTime VR movie from the passed movie containg tiles.*/void CMovieMaker::CreateSingleNodeMovie(	const FSSpec &inTileSpec,	const FSSpec &inMovieSpec,	Int16 inWidth,	Int16 inHeight,	Fixed inPan,	Fixed inTilt,	Fixed inZoom){	OSErr result;	short tileResRefNum = 0;	Movie tileMovie = nil;	short resID = 0;	Track sceneTrack;	Fixed		sceneWidth, sceneHeight;	long sceneTrackID = 0;	Movie panoMovie = nil;	short panoResRefNum = 0;	PanoramaDescriptionHandle panoDesc = nil;	PanoSampleHeaderAtomHandle panoHeader = nil;		try	{		result = ::OpenMovieFile(&inTileSpec, &tileResRefNum, fsRdPerm);		ThrowIfOSErr_(result);				result = ::NewMovieFromFile(&tileMovie, tileResRefNum, &resID, 0, 0, 0);		ThrowIfOSErr_(result);				sceneTrack = ::GetMovieIndTrack(tileMovie, 1);		ThrowIfNil_(sceneTrack);		::SetTrackEnabled(sceneTrack, false);		::GetTrackDimensions(sceneTrack, &sceneWidth, &sceneHeight);		sceneWidth = sceneWidth >> 16;		sceneHeight = sceneHeight >> 16;		sceneTrackID = ::GetTrackID(sceneTrack);				UserData userData;		OSType controllerSubType;		userData = ::GetMovieUserData(tileMovie);		controllerSubType = kPanoMediaType;		::SetUserDataItem(			userData, &controllerSubType, sizeof(controllerSubType), 'ctyp', 0);				panoMovie = ::FlattenMovieData(			tileMovie, flattenDontInterleaveFlatten, &inMovieSpec, 			'vrod', 0, createMovieFileDeleteCurFile); 		 		result = ::OpenMovieFile(&inMovieSpec, &panoResRefNum, fsRdWrPerm); 		ThrowIfOSErr_(result); 		 		::AddMovieResource(panoMovie, panoResRefNum, &resID, 0); 		 		Track panoTrack; 		panoTrack = ::NewMovieTrack( 			panoMovie, ((long)inWidth) << 16, ((long)inHeight) << 16, 0); 		 		TimeScale panoTimeScale; 		panoTimeScale = ::GetMovieTimeScale(panoMovie); 		 		Media panoMedia; 		panoMedia = ::NewTrackMedia(panoTrack, kPanoMediaType, panoTimeScale, 0, 0); 		result = ::GetMoviesError(); 		ThrowIfOSErr_(result); 		 		TimeValue panoDuration; 		panoDuration = ::GetMovieDuration(panoMovie); 		 		panoDesc = (PanoramaDescriptionHandle)::NewHandleClear( 			sizeof (PanoramaDescription)); 		ThrowIfOSErr_(::MemError()); 		 		(*panoDesc)->size = sizeof (PanoramaDescription); 		(*panoDesc)->type = kPanDescType; 		 		(*panoDesc)->sceneTrackID = sceneTrackID; 		(*panoDesc)->hotSpotTrackID = 0; // no hot sport tracks 		 		for (int i = 1; i < 6; i++) 		{ 			(*panoDesc)->reserved3[i] = 0; 			(*panoDesc)->reserved4[i] = 0; 		} 		 		(*panoDesc)->sceneNumFramesX = 1; 		(*panoDesc)->sceneNumFramesY = 24; 		(*panoDesc)->numFrames = 24; 		(*panoDesc)->sceneSizeX = sceneWidth; 		(*panoDesc)->sceneSizeY = sceneHeight * 24; 		 		(*panoDesc)->hPanStart = 0;		(*panoDesc)->hPanEnd = 360 << 16;				float theta = 			180.0 * (atan((*panoDesc)->sceneSizeX * 3.14159 /			(*panoDesc)->sceneSizeY)) / 3.14159;				(*panoDesc)->vPanTop = (Fixed)(theta * 65536);		(*panoDesc)->vPanBottom = (Fixed)(-theta * 65536);				(*panoDesc)->minimumZoom = 0;		(*panoDesc)->maximumZoom = 0;				(*panoDesc)->sceneColorDepth = 32;					// no hot spots		(*panoDesc)->hotSpotNumFramesX = 0;		(*panoDesc)->hotSpotNumFramesY = 0;		(*panoDesc)->hotSpotSizeX = 0;		(*panoDesc)->hotSpotSizeY = 0;		(*panoDesc)->hotSpotColorDepth = 8;					// fill in the panorama header atom data		panoHeader = (PanoSampleHeaderAtomHandle)::NewHandleClear(			sizeof(PanoSampleHeaderAtom));		(*panoHeader)->size = sizeof(PanoSampleHeaderAtom);		(*panoHeader)->type = kPanHeaderType;		(*panoHeader)->nodeID = 0;						// check pan for obvious errors		if (inPan < 0)			inPan = 0;		if (inPan > 360 << 16)			inPan = 360 << 16;					// set the default pan, tilt & zoom		(*panoHeader)->defHPan = inPan;		(*panoHeader)->defVPan = inTilt;		if (inZoom == 0) // default: use some reasonable value			(*panoHeader)->defZoom = (Fixed)(1.5 * theta * 65536);		else			(*panoHeader)->defZoom = inZoom;				(*panoHeader)->commentStrOffset = 0;				result = ::BeginMediaEdits(panoMedia);		ThrowIfOSErr_(result);				result = ::AddMediaSample(panoMedia, (Handle)panoHeader, 0, 			(*panoHeader)->size, panoDuration, 			(SampleDescriptionHandle)panoDesc, 1, 0, nil);		if (result != noErr)		{			::EndMediaEdits(panoMedia);			ThrowIfOSErr_(result);		}				result = ::EndMediaEdits(panoMedia);		ThrowIfOSErr_(result);				::InsertMediaIntoTrack(panoTrack, 0, 0, 			::GetMediaDuration(panoMedia), 1 << 16);		ThrowIfOSErr_(::GetMoviesError());				result = ::UpdateMovieResource(panoMovie, panoResRefNum, resID, 0);		ThrowIfOSErr_(result);				::CloseMovieFile(tileResRefNum);		::DisposeMovie(tileMovie);		::CloseMovieFile(panoResRefNum);		::DisposeMovie(panoMovie);		::DisposeHandle((Handle)panoDesc);		::DisposeHandle((Handle)panoHeader);	}	catch(ExceptionCode inErr)	{		if (tileResRefNum != 0)			::CloseMovieFile(tileResRefNum);		if (tileMovie != nil)			::DisposeMovie(tileMovie);				if (panoResRefNum != 0)			::CloseMovieFile(panoResRefNum);		if (panoMovie != nil)			::DisposeMovie(panoMovie);				if (panoDesc != nil)			::DisposeHandle((Handle)panoDesc);		if (panoHeader != nil)			::DisposeHandle((Handle)panoHeader);				throw(inErr);	}}/* CMovieMaker::CreateTileMovie	Create a QuickTime movie containing 24 tiles created from the passed PICT.*/void CMovieMaker::CreateTileMovie(	PicHandle inPicH,	const FSSpec &inTileSpec,	CodecType inCodec,	CodecQ inSpatialQuality,	Int16 inDepth,	ProgressProc inProgressProc){	Rect		pictRect, tileRect;	LGWorld *tileWorld = nil;	OSErr result;	Movie tileMovie = nil;	short movieResRefNum = 0;	Track tileTrack;	Media tileMedia;	Handle compressedData = nil;	ImageDescriptionHandle imageDesc = nil;	Ptr compressedDataPtr;		try	{			// create a gworld to draw the tiles into		pictRect = tileRect = (*inPicH)->picFrame;		tileRect.bottom = tileRect.top + ((tileRect.bottom - tileRect.top) / 24);		tileWorld = new LGWorld(tileRect, 32, 0, nil, nil);					// create the tile movie file		result = ::CreateMovieFile(			&inTileSpec,			'TVOD', 0, 			createMovieFileDeleteCurFile,			&movieResRefNum,			&tileMovie);		ThrowIfOSErr_(result);					// create a track & media to contain the tiles		tileTrack = ::NewMovieTrack(			tileMovie,			((long)(tileRect.right - tileRect.left)) << 16, 			((long)(tileRect.bottom - tileRect.top)) << 16,			0);		ThrowIfNil_(tileTrack);				tileMedia = ::NewTrackMedia(			tileTrack,			VideoMediaType, 600, 0, 0);		ThrowIfNil_(tileMedia);				result = ::BeginMediaEdits(tileMedia);		ThrowIfOSErr_(result);					// prepare an image description handle		imageDesc = (ImageDescriptionHandle)::NewHandle(4);		ThrowIfOSErr_(::MemError());					// set up the progress record		ICMProgressProcRecord qtproc;		qtproc.progressProc = NewICMProgressProc(QTProgress);		qtproc.progressRefCon = (long)inProgressProc;				long compressedFrameSize;		TimeValue			duration = 60, currentTime;		for (Int16 i = 0; i < 24; i++)		{				// let the user know what is going on			Str255				s1, s2;			CopyPStr("\pCompressing tile ", s1);			::NumToString(i + 1, s2);			ConcatPStr(s1, s2);			ConcatPStr(s1, "\p of 24");			if (inProgressProc(s1))				throw(Exception_UserCancelled);				// draw the picture into the tile gworld			tileWorld->BeginDrawing();			::DrawPicture(inPicH, &pictRect);			tileWorld->EndDrawing();							// offset the pict rect for next time			::OffsetRect(&pictRect, 0, -(tileRect.bottom - tileRect.top));							// prepare a handle for compression			::LockPixels(::GetGWorldPixMap(tileWorld->GetMacGWorld()));			result = GetMaxCompressionSize(				::GetGWorldPixMap(tileWorld->GetMacGWorld()), 				&tileRect, inDepth, inSpatialQuality,				inCodec, anyCodec, &compressedFrameSize);			::UnlockPixels(::GetGWorldPixMap(tileWorld->GetMacGWorld()));			ThrowIfOSErr_(result);			compressedData = ::NewHandle(compressedFrameSize);			ThrowIfOSErr_(::MemError());			::HLock(compressedData);			compressedDataPtr = StripAddress((*compressedData));							// compress the tile			::LockPixels(::GetGWorldPixMap(tileWorld->GetMacGWorld()));			result = ::FCompressImage(				::GetGWorldPixMap(tileWorld->GetMacGWorld()), &tileRect,				0, inSpatialQuality, inCodec, anyCodec,				nil, 0, 0, nil, &qtproc,				imageDesc, compressedDataPtr);			::UnlockPixels(::GetGWorldPixMap(tileWorld->GetMacGWorld()));			if (result == codecAbortErr)				throw(Exception_UserCancelled); // the user did it			ThrowIfOSErr_(result);							// add the tile to the movie			result = ::AddMediaSample(tileMedia, compressedData,0,				(*imageDesc)->dataSize, duration,				(SampleDescriptionHandle)imageDesc, 1, 0, &currentTime);			ThrowIfOSErr_(result);						::DisposeHandle(compressedData);			compressedData = nil;		}				delete tileWorld;		tileWorld = nil;				result = ::EndMediaEdits(tileMedia);		ThrowIfOSErr_(result);				::InsertMediaIntoTrack(			tileTrack,			0, 0,			GetMediaDuration(tileMedia), 1L<<16);		ThrowIfOSErr_(::GetMoviesError());				short resID = 128;		result = ::AddMovieResource(			tileMovie, movieResRefNum, &resID, "\pMovie 1");		ThrowIfOSErr_(result);				::DisposeHandle((Handle)imageDesc);		imageDesc = nil;		::DisposeHandle(compressedData);		compressedData = nil;				::CloseMovieFile(movieResRefNum);		::DisposeMovie(tileMovie);	}	catch(ExceptionCode inErr)	{		if (tileWorld != nil)			delete tileWorld;			// at least try to delete the tile movie file		if (movieResRefNum != 0)		{			::CloseMovieFile(movieResRefNum);			::DeleteMovieFile(&inTileSpec);		}				if (compressedData != nil)			::DisposeHandle(compressedData);		if (imageDesc != nil)			::DisposeHandle((Handle)imageDesc);				if (tileMovie != nil)			::DisposeMovie(tileMovie);				throw(inErr);	}}