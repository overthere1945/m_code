/**
 * @file sns_steng1ax_ver.h
 *
 * Copyright (c) 2022, STMicroelectronics.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the STMicroelectronics nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

/*==============================================================================

    Change Log:

    Mar 25 2025 ST - 1.0.0.0
      - initial release.
      - I3C/I2C support
      - ODR 800 support
      - Non-DAE support
      - Polling/interrupt support
      - Read data from OUT register support
      - 4 STENG1AX under 1 instance/sensor following sns_electro_neuro_graph.proto
    Apr 09 2025 ST - 1.0.0.1
      - Update active configuration sequence
      - New registry logic based on 1 registry instead of 4
      - Include changes from QCOM
    Jul 03 2025 ST - 99.0.0.2
      - Support batching using FIFO
    Jul 15 2025 ST - 1.0.0.2
      - Enable EXT_CLK feature
      - Optimize data acquisition reducing register reads
      - Integrate changes from QCOM
  ============================================================================*/

#ifndef _SNS_STENG1AX_VER_H
#define _SNS_STENG1AX_VER_H

// 32-bit version number represented as major[31:16].minor[15:8].rev[7:0]
#define STENG1AX_MAJOR        1
#define STENG1AX_MINOR        0
#define STENG1AX_REV          2
#define SNS_VERSION_STENG1AX  ((STENG1AX_MAJOR<<16) | (STENG1AX_MINOR<<8) | STENG1AX_REV)

#endif //_SNS_STENG1AX_VER_H

