/**
 * Copyright (C) 2005-06 Nokia Corporation.
 * Contact: Naba Kumar <naba.kumar@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* Useful macros/inlines for use with check */

#define rtcom_fail_unless_strcmp(S1, OP, S2) \
  fail_unless((g_strcmp0((S1), (S2)) OP 0), \
      "Assertion '" #S1 " " #OP " " #S2 "' failed: " \
      "it's not true that %s " #OP " %s", \
      (S1) ? (S1) : "<NULL>", (S2) ? (S2) : "<NULL>")

#define rtcom_fail_unless_intcmp(N1, OP, N2) \
  fail_unless(((N1) OP (N2)), \
      "Assertion '" #N1 " " #OP " " #N2 "' failed: " \
      "it's not true that %i " #OP " %i", \
      (N1), (N2))

#define rtcom_fail_unless_uintcmp(N1, OP, N2) \
  fail_unless(((N1) OP (N2)), \
      "Assertion '" #N1 " " #OP " " #N2 "' failed: " \
      "it's not true that %u " #OP " %u", \
      (N1), (N2))
