#!/usr/bin/env python3
import pandas as pd
from sqlalchemy import create_engine
from prophet import Prophet
import matplotlib.pyplot as plt
from datetime import datetime, timedelta
import logging
import time

# --- Logging Configuration ---
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

# --- MySQL Connection Configuration ---
DB_CONFIG = {
    'user': 'root',  # Default MySQL user
    'password': '',  # Default empty password
    'host': 'localhost',
    'database': 'environmental_monitoring_system',
}

# --- Columns to forecast ---
COLUMNS_TO_FORECAST = ['temperature', 'humidity', 'pressure', 'co2', 'dust', 'aqi']

def fetch_data_from_mysql(limit=None):
    """Connect to MySQL and fetch historical data."""
    start_time = time.time()
    try:
        connection_string = f"mysql+mysqlconnector://{DB_CONFIG['user']}:{DB_CONFIG['password']}@{DB_CONFIG['host']}/{DB_CONFIG['database']}"
        engine = create_engine(connection_string)
        query = f"SELECT timestamp, {', '.join(COLUMNS_TO_FORECAST)} FROM sensor_data ORDER BY timestamp ASC"
        if limit:
            query += f" LIMIT {limit}"

        df = pd.read_sql_query(query, engine)
        logging.info(f"Retrieved {len(df)} records from MySQL.")

        # Convert timestamp to datetime with UTC timezone
        df['timestamp'] = pd.to_datetime(df['timestamp'], utc=True)

        # Infer data frequency
        inferred_freq = pd.infer_freq(df['timestamp'])
        if inferred_freq is None:
            logging.warning("Could not infer data frequency. Using default 'h' frequency.")
        logging.info(f"Inferred data frequency: {inferred_freq}")

        # Force numeric type, handle errors as NaN
        for col in COLUMNS_TO_FORECAST:
            df[col] = pd.to_numeric(df[col], errors='coerce')

        # Check and interpolate missing values if needed
        missing_ratio = df[COLUMNS_TO_FORECAST].isnull().mean()
        for col, ratio in missing_ratio.items():
            if ratio > 0.1:  # 10% threshold
                df[col] = df[col].interpolate(method='linear')
                logging.info(f"Interpolated missing values for column {col}.")
        logging.info(f"Missing data check (NaN):")
        logging.info(df.isnull().sum())

        logging.info(f"Data retrieval took {time.time() - start_time:.2f} seconds")
        return df

    except Exception as err:
        logging.error(f"Error retrieving data: {err}")
        return None
    finally:
        if 'engine' in locals():
            engine.dispose()
            logging.info("MySQL connection closed.")

def preprocess_for_prophet(df, value_column):
    """Prepare DataFrame for Prophet model."""
    start_time = time.time()
    df_prophet = df[['timestamp', value_column]].copy()
    df_prophet.rename(columns={'timestamp': 'ds', value_column: 'y'}, inplace=True)

    # Remove timezone from ds column
    df_prophet['ds'] = df_prophet['ds'].dt.tz_localize(None)

    # Check for infinite values
    if df_prophet['y'].isin([float('inf'), -float('inf')]).any():
        logging.error(f"Data contains infinite values in column {value_column}.")
        return None

    # Remove duplicate timestamps
    duplicates = df_prophet.duplicated(subset=['ds']).sum()
    if duplicates > 0:
        logging.info(f"Removed {duplicates} duplicate records in column {value_column}.")
    df_prophet = df_prophet.drop_duplicates(subset=['ds'], keep='last')

    # Sort by datetime
    df_prophet = df_prophet.sort_values('ds')

    logging.info(f"Data prepared for Prophet ({value_column}): {len(df_prophet)} records.")
    logging.info(f"Time range: {df_prophet['ds'].min()} to {df_prophet['ds'].max()}")
    logging.info(f"Preprocessing took {time.time() - start_time:.2f} seconds")
    return df_prophet

def train_and_forecast(df_prophet, periods=24, freq='h', parameter=''):
    """Train Prophet model and forecast future values."""
    start_time = time.time()
    if df_prophet is None or df_prophet.empty or len(df_prophet) < 2:
        logging.warning(f"Insufficient valid data to train model for {parameter}.")
        return None, None

    # Check data timespan
    time_range = df_prophet['ds'].max() - df_prophet['ds'].min()
    if time_range < timedelta(days=7):
        logging.warning(f"Data spans only {time_range}, too short for seasonal modeling.")

    # Initialize Prophet model with seasonality
    model = Prophet(
        daily_seasonality=True,
        weekly_seasonality=False,  # Disable weekly seasonality due to short data
        yearly_seasonality=False,
        n_changepoints=10  # Optimize performance
    )

    # Train model
    logging.info(f"Started training Prophet model for '{parameter}'...")
    try:
        model.fit(df_prophet)
        logging.info("Training completed.")
    except Exception as e:
        logging.error(f"Error during Prophet training for {parameter}: {e}")
        logging.error(f"Input data for Prophet ('y'):\n{df_prophet['y'].describe()}")
        return None, None

    # Create future dates for forecasting
    inferred_freq = pd.infer_freq(df_prophet['ds'])
    freq = inferred_freq if inferred_freq else 'h'
    future = model.make_future_dataframe(periods=periods, freq=freq)
    logging.info(f"Created future dataframe until: {future['ds'].iloc[-1]} with frequency: {freq}")

    # Generate forecast
    logging.info(f"Starting forecast for {parameter}...")
    forecast = model.predict(future)
    logging.info("Forecast completed.")
    logging.info(f"Training and forecasting took {time.time() - start_time:.2f} seconds")

    return model, forecast

def process_forecast(forecast_data, num_forecast_periods=24):
    """Extract forecast values for the next 24 hours."""
    future_predictions = forecast_data[['ds', 'yhat', 'yhat_lower', 'yhat_upper']].tail(num_forecast_periods)
    return future_predictions

def save_forecast_to_db(final_forecast_df):
    """Save forecast results to MySQL."""
    try:
        connection_string = f"mysql+mysqlconnector://{DB_CONFIG['user']}:{DB_CONFIG['password']}@{DB_CONFIG['host']}/{DB_CONFIG['database']}"
        engine = create_engine(connection_string)
        
        # Create forecast_results table if it doesn't exist
        create_table_sql = """
        CREATE TABLE IF NOT EXISTS forecast_results (
            timestamp DATETIME PRIMARY KEY,
            temperature_forecast FLOAT,
            humidity_forecast FLOAT,
            pressure_forecast FLOAT,
            co2_forecast FLOAT,
            dust_forecast FLOAT,
            aqi_forecast FLOAT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
        """
        engine.execute(create_table_sql)

        # Prepare and execute insert/update
        final_forecast_df.to_sql('forecast_results', engine, if_exists='replace', index=False)
        logging.info(f"Saved {len(final_forecast_df)} records to forecast_results table.")

    except Exception as db_err:
        logging.error(f"Error saving to MySQL: {db_err}")
    finally:
        if 'engine' in locals():
            engine.dispose()
            logging.info("MySQL connection closed.")

def main():
    logging.info("=== Starting Forecasting Process ===")
    overall_start_time = time.time()

    # Fetch historical data
    df_history = fetch_data_from_mysql()

    if df_history is None or df_history.empty:
        logging.error("Could not retrieve data from MySQL. Aborting.")
        return

    all_forecasts = {}
    forecast_start_time = df_history['timestamp'].iloc[-1] + timedelta(hours=1)

    # Process each parameter
    for column in COLUMNS_TO_FORECAST:
        logging.info(f"--- Processing {column} ---")

        # Preprocess
        df_train = preprocess_for_prophet(df_history, column)
        if df_train is None:
            logging.warning(f"Could not preprocess data for: {column}")
            continue

        # Train and forecast
        model, forecast_result = train_and_forecast(df_train, periods=24, freq='h', parameter=column)
        if model is None or forecast_result is None:
            logging.warning(f"Could not generate forecast for: {column}")
            continue

        # Get next 24h predictions
        future_preds = process_forecast(forecast_result, num_forecast_periods=24)
        all_forecasts[column] = future_preds
        logging.info(f"Forecast for {column} completed.")

        # Plot and save graph
        try:
            fig = model.plot(forecast_result)
            plt.title(f"{column} Forecast")
            plt.xlabel("Time")
            plt.ylabel(column)
            plt.savefig(f"{column}_forecast.png")
            logging.info(f"Saved forecast plot: {column}_forecast.png")
        except Exception as plot_err:
            logging.error(f"Error saving plot {column}_forecast.png: {plot_err}")
        finally:
            plt.close(fig)

    # Display and save combined results
    if all_forecasts:
        logging.info("\n=== 24-Hour Forecast Results ===")
        final_forecast_df = pd.DataFrame()
        
        if COLUMNS_TO_FORECAST[0] in all_forecasts:
            final_forecast_df['timestamp'] = all_forecasts[COLUMNS_TO_FORECAST[0]]['ds']
        else:
            logging.error("No forecasts available to create combined DataFrame.")
            return

        for column, forecast_df in all_forecasts.items():
            final_forecast_df[f'{column}_forecast'] = forecast_df['yhat'].round(2)

        # Round timestamps to hours
        final_forecast_df['timestamp'] = pd.to_datetime(final_forecast_df['timestamp']).dt.floor('h')

        # Sort by timestamp
        final_forecast_df = final_forecast_df.sort_values(by='timestamp')

        # Print results
        print("\nForecast Results:")
        print(final_forecast_df.to_string())

        # Save to CSV
        try:
            csv_filename = f"forecast_results_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
            final_forecast_df.to_csv(csv_filename, index=False)
            logging.info(f"Saved forecast results to: {csv_filename}")
        except Exception as save_err:
            logging.error(f"Error saving CSV file: {save_err}")

        # Save to MySQL
        save_forecast_to_db(final_forecast_df)

    else:
        logging.warning("No forecast results were generated.")

    logging.info(f"Total process time: {time.time() - overall_start_time:.2f} seconds")
    logging.info("=== Forecasting Process Completed ===")

if __name__ == "__main__":
    main()
