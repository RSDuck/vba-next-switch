/* 7zExtract.c -- Extracting from 7z archive
2008-11-23 : Igor Pavlov : Public domain */

#include "7zCrc.h"
#include "7zDecode.h"
#include "7zExtract.h"

SRes SzAr_Extract(
    const CSzArEx *p,
    ILookInStream *inStream,
    UInt32 fileIndex,
    UInt32 *blockIndex,
    Byte **outBuffer,
    size_t *outBufferSize,
    size_t *offset,
    size_t *outSizeProcessed,
    ISzAlloc *allocMain,
    ISzAlloc *allocTemp)
{
  UInt32 folderIndex = p->FileIndexToFolderIndexMap[fileIndex];
  SRes res = SZ_OK;
  *offset = 0;
  *outSizeProcessed = 0;
  if (folderIndex == (UInt32)-1)
  {
    IAlloc_Free(allocMain, *outBuffer);
    *blockIndex = folderIndex;
    *outBuffer = 0;
    *outBufferSize = 0;
    return SZ_OK;
  }

  if (*outBuffer == 0 || *blockIndex != folderIndex)
  {
    CSzFolder *folder = p->db.Folders + folderIndex;
    UInt64 unpackSizeSpec = SzFolder_GetUnpackSize(folder);
    size_t unpackSize = (size_t)unpackSizeSpec;
    UInt64 startOffset = SzArEx_GetFolderStreamPos(p, folderIndex, 0);

    if (unpackSize != unpackSizeSpec)
      return SZ_ERROR_MEM;
    *blockIndex = folderIndex;
    IAlloc_Free(allocMain, *outBuffer);
    *outBuffer = 0;
    
    RINOK(LookInStream_SeekTo(inStream, startOffset));
    
    if (res == SZ_OK)
    {
      *outBufferSize = unpackSize;
      if (unpackSize != 0)
      {
        *outBuffer = (Byte *)IAlloc_Alloc(allocMain, unpackSize);
        if (*outBuffer == 0)
          res = SZ_ERROR_MEM;
      }
      if (res == SZ_OK)
      {
        res = SzDecode(p->db.PackSizes +
          p->FolderStartPackStreamIndex[folderIndex], folder,
          inStream, startOffset,
          *outBuffer, unpackSize, allocTemp);
        if (res == SZ_OK)
        {
          if (folder->UnpackCRCDefined)
          {
            if (CrcCalc(*outBuffer, unpackSize) != folder->UnpackCRC)
              res = SZ_ERROR_CRC;
          }
        }
      }
    }
  }
  if (res == SZ_OK)
  {
    UInt32 i;
    CSzFileItem *fileItem = p->db.Files + fileIndex;
    *offset = 0;
    for (i = p->FolderStartFileIndex[folderIndex]; i < fileIndex; i++)
      *offset += (UInt32)p->db.Files[i].Size;
    *outSizeProcessed = (size_t)fileItem->Size;
    if (*offset + *outSizeProcessed > *outBufferSize)
      return SZ_ERROR_FAIL;
    {
      if (fileItem->FileCRCDefined)
      {
        if (CrcCalc(*outBuffer + *offset, *outSizeProcessed) != fileItem->FileCRC)
          res = SZ_ERROR_CRC;
      }
    }
  }
  return res;
}





#ifdef _LZMA_OUT_READ
// similar to SzExtract but needs less memory
SRes SzExtract2(
	const CSzArEx *db,
	ILookInStream *inStream,
    UInt32 fileIndex,
    UInt32 *blockIndex,
    Byte **outBuffer,
    size_t *outBufferSize,
    size_t *offset,
    size_t *outSizeProcessed,
    ISzAlloc *allocMain,
    ISzAlloc *allocTemp)
{
  UInt32 folderIndex = db->FileIndexToFolderIndexMap[fileIndex];
  SRes res = SZ_OK;
  *offset = 0;
  *outSizeProcessed = 0;
  if (folderIndex == (UInt32)-1)
  {
    *blockIndex = folderIndex;
    #ifndef GEKKO1
    //allocMain->Free(*outBuffer);
    IAlloc_Free(allocMain, *outBuffer);
    *outBuffer = 0;
    #endif
    *outBufferSize = 0;
    return SZ_OK;
  }

	#define CFileSize UInt64
//  if (*outBuffer == 0 || *blockIndex != folderIndex)
//  {
    CSzFolder *folder = db->db.Folders + folderIndex;
    CFileSize unPackSize = SzFolder_GetUnpackSize(folder);
    #ifndef _LZMA_IN_CB1
    CFileSize packSize = 0;
    res = SzArEx_GetFolderFullPackSize(db, folderIndex, &packSize);
    Byte *inBuffer = 0;
    size_t processedSize;
    #endif
    *blockIndex = folderIndex;
    #ifndef GEKKO1
    IAlloc_Free(allocMain, *outBuffer);
    *outBuffer = 0;
    #endif

    RINOK(LookInStream_SeekTo(inStream, SzArEx_GetFolderStreamPos(db, folderIndex, 0)));

    #ifndef _LZMA_IN_CB1
    if (packSize != 0)
    {
      inBuffer = (Byte *)IAlloc_Alloc(allocTemp, (size_t)packSize);
      if (inBuffer == 0)
        return SZ_ERROR_MEM;
    }

    res = LookInStream_Read2(inStream, inBuffer, (size_t)packSize, &processedSize);
    if (res == SZ_OK && processedSize != (size_t)packSize)
      res = SZ_ERROR_FAIL;
    #endif
    if (res == SZ_OK)
    {
          // calculate file offset and filesize
    	CSzFileItem *fileItem = db->db.Files + fileIndex;
          UInt32 i;
          *offset = 0;
          for(i = db->FolderStartFileIndex[folderIndex]; i < fileIndex; i++)
                *offset += (UInt32)db->db.Files[i].Size;
          *outSizeProcessed = (size_t)fileItem->Size;
      *outBufferSize = (size_t)fileItem->Size;
      if (unPackSize != 0)
      {
                #ifndef GEKKO1
        *outBuffer = (Byte *)IAlloc_Alloc(allocMain, (size_t)fileItem->Size);
        if (*outBuffer == 0)
          res = SZ_ERROR_MEM;
        #endif
      }
      if (res == SZ_OK)
      {

    	  /*SRes SzDecode(const UInt64 *packSizes, const CSzFolder *folder,
    	      ILookInStream *inStream, UInt64 startPos,
    	      Byte *outBuffer, size_t outSize, ISzAlloc *allocMain)*/

    	 //HAL
        size_t outRealSize = *outBufferSize;
        /*res = SzDecode2(db->db.PackSizes + db->FolderStartPackStreamIndex[folderIndex],
        				folder,
#ifdef _LZMA_IN_CB
						inStream,
#else
						inBuffer,
#endif
						*outBuffer,
						(size_t)unPackSize,
						&outRealSize,
						allocTemp,
						offset,
						outSizeProcessed
                  	  );*/
        res = SzDecode(db->db.PackSizes + db->FolderStartPackStreamIndex[folderIndex],
        				folder,
#ifdef _LZMA_IN_CB1
						inStream,
#else
						inBuffer,
#endif
						offset,
						*outBuffer,
						*outBufferSize,
						allocTemp
                  	  );
		  *outSizeProcessed = outRealSize;
/*        if (res == SZ_OK) // we can't validate the CRC of the whole data stream because we only extracted the wanted file
        {
          if (outRealSize == (size_t)unPackSize)
          {
            if (folder->UnPackCRCDefined)
            {
              if (!CrcVerifyDigest(folder->UnPackCRC, *outBuffer, (size_t)unPackSize))
                res = SZE_FAIL;
            }
          }
          else
            res = SZE_FAIL;
        }*/
      }
//    }
    #ifndef _LZMA_IN_CB1
    //allocTemp->Free(inBuffer);
    IAlloc_Free(allocTemp, *outBuffer);
    #endif
  }
  if (res == SZ_OK)
  {
/*    UInt32 i;
    CFileItem *fileItem = db->Database.Files + fileIndex;
    *offset = 0;
    for(i = db->FolderStartFileIndex[folderIndex]; i < fileIndex; i++)
      *offset += (UInt32)db->Database.Files[i].Size;
    *outSizeProcessed = (size_t)fileItem->Size;*/
    //CFileItem *fileItem = db->Database.Files + fileIndex;
    if (/**offset +*/ *outSizeProcessed > *outBufferSize)
      return SZ_ERROR_FAIL;
    {
      //if (fileItem->IsFileCRCDefined)
      //{
       // if (!CrcVerifyDigest(fileItem->FileCRC, *outBuffer/* + *offset*/, *outSizeProcessed))
        //  res = SZE_CRC_ERROR; // why does SzExtract return SZE_FAIL when we can return SZE_CRC_ERROR?
      //}
    }
  }

  // change *offset to 0 because SzExtract normally decompresses the whole solid block
  // and sets *offset to the offset of the wanted file.
  // SzDecode2 does only copy the needed file to the output buffer and has to set *offset
  // to 0 to ensure compatibility with SzExtract
  *offset = 0;
  return res;
}
#endif
