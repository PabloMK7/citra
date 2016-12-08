// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace Service {

class Interface;

namespace AM {

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

/// Initialize AM service
void Init();

/// Shutdown AM service
void Shutdown();

} // namespace AM
} // namespace Service
