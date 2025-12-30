from datetime import datetime

today = datetime.now()
print(f"sensing_{today.year}{today.month:02}{today.day:02}.csv")
