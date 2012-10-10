<?php
/**
 * Check to see if a user is also the purchaser of the gift certificate.
 *
 * @param integer $certificate_id
 *   The incrementally-numbered gift certificate identification number.
 * @param object $user
 *   A valid Drupal user object.
 *
 * @return bool TRUE if the user has purchased this gift certificate, else FALSE.
 */
function uc_gift_certificate_is_purchaser($certificate_id, $user){
  $certificate = uc_gift_certificate_load($certificate_id);
  if ($certificate->purchaser_id == $user->uid) {
    return TRUE;
  }
  return FALSE;
}

/**
 * Helper function to get username based on uid.
 *
 * Called by admin.inc's uc_gift_certificate_add_form_validate()
 */
// TODO Turn this into a true API call: $user = user_load(array('uid' => $uid));
function uc_gift_certificate_uid_from_username($name){
  $result = db_query("SELECT uid FROM {users} WHERE name = '%s'", $name);
  if ($user = db_fetch_object($result)) {
    return $user->uid;
  }
  return false;
}

/**
 * Return the number of certificates a user has.
 */
function uc_gift_certificate_certs_issued($uid) {
  $res = db_fetch_object(db_query("SELECT COUNT(*) as n FROM {uc_gift_certificates} WHERE user_id  = %d AND remaining_value > 0", $uid));
  return $res->n;
}

/**
 * Give the user some certificates.
 */
function uc_gift_certificates_gift($user, $amount) {
  $cert_code = uc_gift_certificate_mKey();
  $query = db_query("INSERT INTO {uc_gift_certificates} (name, original_value, remaining_value, user_id) VALUES ('%s', '%s', %d)", 'Promo certificate', $amount, $amount, $user->uid);
  $certificate_id = db_last_insert_id('uc_gift_certificates', 'certificate_id');
  $cert_code = $certificate_id."-".$cert_code;
  db_query("UPDATE {uc_gift_certificates} SET cert_code = '%s' WHERE certificate_id = %d", $cert_code, $certificate_id);
  uc_order_comment_save($order->order_id, $user->uid, t('Added gift certificate !code worth !value.', array('!code' => $cert_code, '!value' => uc_currency_format($amount))), 'admin');
  return array('cert_code' => $cert_code, 'cert_id' => $certificate_id);
}

/**
 * Reverse the gift certificates (refund back to store) if an order has been canceled.
 */
function uc_gift_certificate_reverse($order_id) {

}

/**
 * Fetch all of a user's gift certificate remaining values.
 *
 * Called by panes.inc's uc_checkout_pane_gift_certificate().
 *
 * @param integer $uid
 *   The user id of the account which may have been issued some gift certificates.
 */
function uc_gift_certificate_load_certificates($uid) {
  if (variable_get('uc_gift_certificate_anonymous_recipients', '') == '1') {
    // For purposes of credit during checkout, issued certificates should be ones
    // that have not been purchased by this user, but which *are* issued to this
    // user. In other words, the purchaser_id and user_id should not be identical,
    // or it means that the cert was purchased by this user and not yet given
    // away or redeemed so we cannot allow them to re-redeem it during checkout.
    $cert_array = db_query("SELECT * FROM {uc_gift_certificates} WHERE user_id = %d", $uid);
    while ($certificate = db_fetch_object($cert_array)) {
      if ($certificate->purchaser_id != $certificate->user_id) {
        $return[$certificate->certificate_id] = $certificate->name.": ".$certificate->remaining_value;
      }
    }
    return $return;
  }
  else {
    $cert_array = db_query("SELECT * FROM {uc_gift_certificates} WHERE user_id = %d", $uid);
    while ($cert = db_fetch_object($cert_array)) {
      $return[$cert->certificate_id] = $cert->name.": ".$cert->remaining_value;
    }
    return $return;
  }
}

/**
 * load users.
 */
function uc_gift_certificate_load_users() {
  $user_array = db_query("SELECT uid, name FROM {users} ORDER BY name ASC");
  while ($users = db_fetch_object($user_array)) {
    $user_return[$users->uid] = $users->name;
  }
  return $user_return;
}

/**
 * Validate a specific Gift Certificate code.
 */
function uc_gift_certificate_validate_user($user) {
  // The following query grabs the sum of available gift certificates for this specific user.
  $coupon_data = db_fetch_object(db_query("SELECT *, SUM(remaining_value) AS remaining_value FROM {uc_gift_certificates} WHERE user_id = %d", $user));

  $coupon_result = new stdClass();

  if ($coupon_data) {
    $coupon_result->valid = TRUE;

    foreach (uc_cart_get_contents() as $item) {
      $cart_total += $item->price * $item->qty;
    }

    // Grab the current order session and include all line_items into cart total.
    $order = uc_order_load($_SESSION['cart_order']);
    if ($order->order_id) {
      $line_items = db_fetch_object(db_query("SELECT SUM(amount) AS amount FROM {uc_order_line_items} WHERE order_id = %d AND type != 'gift_certificate'", $order->order_id));
      $cart_total += $line_items->amount;
    }

    $coupon_result->valid = TRUE;
    $coupon_result->total = $coupon_data->remaining_value;

    $coupon_result->amount = $coupon_data->remaining_value;

    // Find out if our certificate amount is equal to or more than the current total.
    if ($coupon_result->amount >= $cart_total) {
      drupal_set_message($coupon_result->amount .'covers total order!');
      $coupon_result->amount = $cart_total;
    }

    $coupon_result->title = t('Gift Certificates - DISCOUNT: @code', array('@code' => $coupon_result->amount));
  }
  else {
    // The coupon was not found, inactive, or is expired in the list of coupons
    $coupon_result->valid = FALSE;
  }
  return $coupon_result;
}

/**
 * Implementation of hook_user().
 *
 * Form for working with user settings
 */
function uc_gift_certificate_user($op, $edit, &$account, $category = NULL) {
  global $user;

  switch ($op) {
    case 'view':
      if (($account->uid == $user->uid || user_access('administer gift certificates')) && $total = uc_gift_certificate_total($account->uid)) {
        $account->content['gift_certificates'] = array(
          '#type' => 'user_profile_category',
          '#weight' => -4, // Underneath "view order history"
          '#title' => t('Gift Certificates'),
        );
        $account->content['gift_certificates']['certs_issued'] = array(
          '#type' => 'user_profile_item',
          '#title' => t('Your Gift Certificates'),
          '#value' => t('You have <strong>!value</strong> worth of gift certificates. They can be redeemed when you check out.', array('!value' => uc_currency_format($total))),
          '#attributes' => array('class' => 'uc-gift-certificate-message'),
        );
      }
      break;

    case 'form':
      if ($category == 'account') {
        $form['gift_certificates'] = array(
          '#type' => 'fieldset',
          '#title' => t('My Gift Certificates'),
          '#collapsible' => FALSE,
          '#collapsed' => FALSE,
          '#weight' => 2,
          '#attributes' => array('class' => 'uc-gift-certificate-user'),
        );

        $value = uc_gift_certificate_total($account->uid);
        $form['gift_certificates']['certs_issued'] = array(
          '#value' => t("You have <strong>!value</strong> worth of gift certificates stored in your account. They can be redeemed when you check out.", array('!value' => uc_currency_format($value))),
          '#attributes' => array('class' => 'uc-gift-certificate-message'),
        );

        $form['gift_certificates']['certs_new'] = array(
          '#type' => 'textfield',
          '#title' => t('Redeem a gift certificate code'),
          '#description' => t('If you have a gift certificate code, you can enter it here.<br/>
            Its value will be added to your account, and will be available to use during Checkout.'),
          '#size' => 20,
        );

        return $form;
      }
      break;

    case 'update':
      if (isset($edit['certs_new'])) {
        $form_state = array('values' => array('certificate_code' => $edit['certs_new']));
        uc_gift_certificate_cert_code_form_submit('account', $form_state);
      }
      break;
  }
}

/**
 * Helper function for the autocomplete username field.
 */
function uc_gift_certificate_username_from_uid($uid){
  $result = db_query("SELECT name FROM {users} WHERE uid = %d", $uid);
  if ($user = db_fetch_object($result)) {
    return $user->name;
  }
  return false;
}

/**
 * Form to enter a gift certificate (for a product).
 */
function uc_gift_certificate_issue_form($form_state, $certificate_id) {
  global $user;
  if (!uc_gift_certificate_is_purchaser($certificate_id, $user)){
    // Redirect to homepage.
    drupal_goto();
  }
  $form['certificate_id'] = array('#type' => 'value',
      '#value' => $certificate_id,
  );
  $form['email'] = array(
    '#type' => 'textfield',
    '#title' => t('Email'),
    '#description' => t('Enter the email of the person you would like to give the gift certificate to.'),
    '#default_value' =>  '',
  );
  $form['op'] = array('#type' => 'submit',
    '#value' => t('Submit'),
    '#weight' => 2,
  );
  return $form;
}

/**
 * Implementation of hook_submit().
 */
function uc_gift_certificate_issue_form_submit($form, &$form_state) {
  $mail = $form_state['values']['email'];
  $certificate_id = $form_state['values']['certificate_id'];
  uc_gift_certificate_issue_cert($mail, $certificate_id);
  uc_gift_certificate_mail_cert($mail, $certificate_id);

  return 'my_gift_certificates';
}

