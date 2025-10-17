#!/usr/bin/env python3
"""
Test Futu OpenD connection with official Python SDK
"""

from futu import OpenQuoteContext

# Connect to OpenD
quote_ctx = OpenQuoteContext(host='127.0.0.1', port=11111)

# Initialize connection
ret, data = quote_ctx.get_global_state()
if ret == 0:
    print("Success!")
    print(data)
else:
    print(f"Failed: {data}")

quote_ctx.close()
