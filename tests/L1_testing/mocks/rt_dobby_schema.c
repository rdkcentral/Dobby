/* If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2023 Synamedia
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
*/

#include "rt_dobby_schema.h"

void free_rt_dobby_schema_hooks (rt_dobby_schema_hooks *ptr)
{
}
char *
rt_dobby_schema_generate_json (const rt_dobby_schema *ptr, const struct parser_context *ctx, parser_error *err)
{
   char *json_buf = NULL;
   return json_buf;
}
rt_dobby_schema *
rt_dobby_schema_parse_file (const char *filename, const struct parser_context *ctx, parser_error *err)
{
   rt_dobby_schema *ptr = NULL;
   return ptr;
}
void
free_rt_dobby_schema (rt_dobby_schema *ptr)
{
}
void free_rt_defs_plugins_legacy_plugins (rt_defs_plugins_legacy_plugins *ptr)
{
}
