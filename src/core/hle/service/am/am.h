// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "common/common_types.h"

namespace Service {
namespace FS {
enum class MediaType : u32;
}
}

namespace Service {

class Interface;

namespace AM {

/**
 * Get the .tmd path for a title
 * @param media_type the media the title exists on
 * @param tid the title ID to get
 * @returns string path to the .tmd file if it exists, otherwise a path to create one is given.
 */
std::string GetTitleMetadataPath(Service::FS::MediaType media_type, u64 tid);

/**
 * Get the .app path for a title's installed content index.
 * @param media_type the media the title exists on
 * @param tid the title ID to get
 * @param index the content index to get
 * @returns string path to the .app file
 */
std::string GetTitleContentPath(Service::FS::MediaType media_type, u64 tid, u16 index = 0);

/**
 * Get the folder for a title's installed content.
 * @param media_type the media the title exists on
 * @param tid the title ID to get
 * @returns string path to the title folder
 */
std::string GetTitlePath(Service::FS::MediaType media_type, u64 tid);

/**
 * Get the title/ folder for a storage medium.
 * @param media_type the storage medium to get the path for
 * @returns string path to the folder
 */
std::string GetMediaTitlePath(Service::FS::MediaType media_type);

/**
 * Scans the for titles in a storage medium for listing.
 * @param media_type the storage medium to scan
 */
void ScanForTitles(Service::FS::MediaType media_type);

/**
 * Scans all storage mediums for titles for listing.
 */
void ScanForAllTitles();

/**
 * AM::GetNumPrograms service function
 * Gets the number of installed titles in the requested media type
 *  Inputs:
 *      0 : Command header (0x00010040)
 *      1 : Media type to load the titles from
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 *      2 : The number of titles in the requested media type
 */
void GetNumPrograms(Service::Interface* self);

/**
 * AM::FindContentInfos service function
 *  Inputs:
 *      1 : MediaType
 *    2-3 : u64, Title ID
 *      4 : Content count
 *      6 : Content IDs pointer
 *      8 : Content Infos pointer
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 */
void FindContentInfos(Service::Interface* self);

/**
 * AM::ListContentInfos service function
 *  Inputs:
 *      1 : Content count
 *      2 : MediaType
 *    3-4 : u64, Title ID
 *      5 : Start Index
 *      7 : Content Infos pointer
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 *      2 : Number of content infos returned
 */
void ListContentInfos(Service::Interface* self);

/**
 * AM::DeleteContents service function
 *  Inputs:
 *      1 : MediaType
 *    2-3 : u64, Title ID
 *      4 : Content count
 *      6 : Content IDs pointer
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 */
void DeleteContents(Service::Interface* self);

/**
 * AM::GetProgramList service function
 * Loads information about the desired number of titles from the desired media type into an array
 *  Inputs:
 *      1 : Title count
 *      2 : Media type to load the titles from
 *      4 : Title IDs output pointer
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 *      2 : The number of titles loaded from the requested media type
 */
void GetProgramList(Service::Interface* self);

/**
 * AM::GetProgramInfos service function
 *  Inputs:
 *      1 : u8 Mediatype
 *      2 : Total titles
 *      4 : TitleIDList pointer
 *      6 : TitleList pointer
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 */
void GetProgramInfos(Service::Interface* self);

/**
 * AM::GetDataTitleInfos service function
 * Wrapper for AM::GetProgramInfos
 *  Inputs:
 *      1 : u8 Mediatype
 *      2 : Total titles
 *      4 : TitleIDList pointer
 *      6 : TitleList pointer
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 */
void GetDataTitleInfos(Service::Interface* self);

/**
 * AM::ListDataTitleTicketInfos service function
 *  Inputs:
 *      1 : Ticket count
 *    2-3 : u64, Title ID
 *      4 : Start Index?
 *      5 : (TicketCount * 24) << 8 | 0x4
 *      6 : Ticket Infos pointer
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 *      2 : Number of ticket infos returned
 */
void ListDataTitleTicketInfos(Service::Interface* self);

/**
 * AM::GetNumContentInfos service function
 *  Inputs:
 *      0 : Command header (0x100100C0)
 *      1 : MediaType
 *    2-3 : u64, Title ID
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 *      2 : Number of content infos plus one
 */
void GetNumContentInfos(Service::Interface* self);

/**
 * AM::DeleteTicket service function
 *  Inputs:
 *    1-2 : u64, Title ID
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 */
void DeleteTicket(Service::Interface* self);

/**
 * AM::GetNumTickets service function
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 *      2 : Number of tickets
 */
void GetNumTickets(Service::Interface* self);

/**
 * AM::GetTicketList service function
 *  Inputs:
 *      1 : Number of TicketList
 *      2 : Number to skip
 *      4 : TicketList pointer
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 *      2 : Total TicketList
 */
void GetTicketList(Service::Interface* self);

/**
 * AM::QueryAvailableTitleDatabase service function
 *  Inputs:
 *      1 : Media Type
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 *      2 : Boolean, database availability
 */
void QueryAvailableTitleDatabase(Service::Interface* self);

/**
 * AM::CheckContentRights service function
 *  Inputs:
 *      1-2 : Title ID
 *      3 : Content Index
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 *      2 : Boolean, whether we have rights to this content
 */
void CheckContentRights(Service::Interface* self);

/**
 * AM::CheckContentRightsIgnorePlatform service function
 *  Inputs:
 *      1-2 : Title ID
 *      3 : Content Index
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 *      2 : Boolean, whether we have rights to this content
 */
void CheckContentRightsIgnorePlatform(Service::Interface* self);

/// Initialize AM service
void Init();

/// Shutdown AM service
void Shutdown();

} // namespace AM
} // namespace Service
