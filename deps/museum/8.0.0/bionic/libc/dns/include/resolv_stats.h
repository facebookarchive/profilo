/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _RES_STATS_H
#define _RES_STATS_H

#include <sys/socket.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "resolv_params.h"

#define RCODE_INTERNAL_ERROR    254
#define RCODE_TIMEOUT           255

/*
 * Resolver reachability statistics and run-time parameters.
 */

struct __res_sample {
    time_t			at;    // time in s at which the sample was recorded
    uint16_t			rtt;   // round-trip time in ms
    uint8_t			rcode; // the DNS rcode or RCODE_XXX defined above
};

struct __res_stats {
    // Stats of the last <sample_count> queries.
    struct __res_sample		samples[MAXNSSAMPLES];
    // The number of samples stored.
    uint8_t			sample_count;
    // The next sample to modify.
    uint8_t			sample_next;
};

/* Calculate the round-trip-time from start time t0 and end time t1. */
extern int
_res_stats_calculate_rtt(const struct timespec* t1, const struct timespec* t0);

/* Initialize a sample for calculating server reachability statistics. */
extern void
_res_stats_set_sample(struct __res_sample* sample, time_t now, int rcode, int rtt);

/* Returns true if the server is considered unusable, i.e. if the success rate is not lower than the
 * threshold for the stored stored samples. If not enough samples are stored, the server is
 * considered usable.
 */
extern bool
_res_stats_usable_server(const struct __res_params* params, struct __res_stats* stats);

__BEGIN_DECLS
/* Aggregates the reachability statistics for the given server based on on the stored samples. */
extern void
android_net_res_stats_aggregate(struct __res_stats* stats, int* successes, int* errors,
        int* timeouts, int* internal_errors, int* rtt_avg, time_t* last_sample_time)
    __attribute__((visibility ("default")));

extern int
android_net_res_stats_get_info_for_net(unsigned netid, int* nscount,
        struct sockaddr_storage servers[MAXNS], int* dcount, char domains[MAXDNSRCH][MAXDNSRCHPATH],
        struct __res_params* params, struct __res_stats stats[MAXNS])
    __attribute__((visibility ("default")));

/* Returns an array of bools indicating which servers are considered good */
extern void
android_net_res_stats_get_usable_servers(const struct __res_params* params,
        struct __res_stats stats[], int nscount, bool valid_servers[])
    __attribute__((visibility ("default")));
__END_DECLS

#endif  // _RES_STATS_H
