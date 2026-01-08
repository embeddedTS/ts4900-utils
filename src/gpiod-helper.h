// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2023 Kent Gibson <warthog618@gmail.com>

/* The following helper functions were pulled mostly verbatim from the libgpiod
 * examples. More information on each function is included in its header.
 */

#pragma once

#include <gpiod.h>
#include <stdio.h>

/* Request a single line as an input.
 * Returns struct gpiod_line_request* on success or NULL on failure.
 * Unfortunately most libgpiod functions don't set errno on failure.
 *
 * Pulled from the v2.2.x branch:
 * https://github.com/brgl/libgpiod/blob/v2.2.x/examples/get_line_value.c
 */

struct gpiod_line_request *request_input_line(const char *chip_path,
					      unsigned int offset,
					      const char *consumer);

/* Request a single line as an output.
 * Returns struct gpiod_line_request* on success or NULL on failure.
 * Unfortunately most libgpiod functions don't set errno on failure.
 *
 * Pulled from the v2.2.x branch:
 * https://github.com/brgl/libgpiod/blob/v2.2.x/examples/toggle_line_value.c
 */
struct gpiod_line_request *
request_output_line(const char *chip_path, unsigned int offset,
		    enum gpiod_line_value value, const char *consumer);
