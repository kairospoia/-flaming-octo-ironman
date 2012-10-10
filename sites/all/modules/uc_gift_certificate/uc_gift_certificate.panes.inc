<?php
/**
 * Our checkout panes.
 */
function uc_gift_certificate_checkout_pane() {
  $panes[] = array(
    'id' => 'certificate-discount',
    //'callback' => 'uc_gift_certificate_pane_gc_discount',
    'callback' => 'uc_gift_certificate_checkout_enter_code',
    'title' => t('Redeem a Gift Certificate'),
    'desc' => t('Allows shoppers to redeem a gift certificate code during checkout.'),
    'weight' => 5,
    'process' => TRUE,
  );
  $panes[] = array(
    'id' => 'gift_certificate',
    'callback' => 'uc_checkout_pane_gift_certificate',
    'title' => t('Gift Certificates'),
    'desc' => t('Allows shoppers to see that they have gift certificate credit(s) during checkout.'),
    'weight' => 5,
    'process' => TRUE,
  );

  return $panes;
}

/**
 * Checkout pane that displays how much in gift certificates a user has.
 */
function uc_checkout_pane_gift_certificate($op, &$order, $arg2) {
  global $user;

  switch ($op) {
    case 'settings':
      $form['uc_gift_certificate_pane_description'] = array(
        '#type' => 'textarea',
        '#title' => t('Checkout pane message'),
        '#default_value' => variable_get('uc_gift_certificate_pane_description', 'Enter a Gift Certificate code for this order.'),
      );

      $form['uc_gift_certificate_pane_msg'] = array(
        '#type' => 'textarea',
        '#title' => t('Checkout Pane Message'),
        '#default_value' => variable_get('uc_gift_certificate_pane_msg', uc_gift_certificate_get_setting_defaults('uc_gift_certificate_pane_msg')),
        '#description' => t('Gift certificate message in the checkout pane.'),
      );
      return $form;

    case 'view':
      // See if we are editing an order or creating it.
      $uid = empty($order->uid) ? $user->uid : $order->uid;
      if (!uc_gift_certificate_total($uid)){
        // Do not show the pane in this case.
        return;
      }

      $cert_arr = uc_gift_certificate_load_certificates($uid);
      $total = uc_gift_certificate_total($uid);
      $description = t('');
      $value = variable_get('uc_gift_certificate_pane_msg', uc_gift_certificate_get_setting_defaults('uc_gift_certificate_pane_msg'));
      $variables = array('!tot' => uc_currency_format($total));
      $value = strtr($value, $variables);
      $contents['gift_certificate_total'] = array('#value' => $value);
      return array('description' => $description, 'contents' => $contents);
  }
}

/*
 * Checkout pane that allows the entry of a gift certificate code.
 */
function uc_gift_certificate_pane_gc_discount($op, &$arg1, $arg2) {
  switch ($op) {
    case 'view':
      drupal_add_js(drupal_get_path('module', 'uc_gift_certificate') .'/uc_gift_certificate.js');
      $description = variable_get('uc_gift_certificate_pane_description', 'Enter a gift certificate code for this order.');
      $contents['certificate_code'] = array(
        '#type' => 'textfield',
        '#title' => t('Certificate Code'),
        '#default_value' => $arg1->data['gift_certificate'],
        '#size' => 25,
      );
      return array('description' => $description, 'contents' => $contents);
    case 'process':
      $arg1->data['certificate_code'] = $arg2['certificate_code'];
      return TRUE;
    case 'review':
      break;
  }
}

/**
 * Checkout Pane callback function for the 'enter a gift certificate code' textfield.
 *
 * Used to display a form in the checkout process so that customers
 * can enter discount coupons.
 *
 * @param string $op
 *   State of the action taking place. Switchable.
 * @param object $order
 *   The order that is being placed during checkout.
 * @param array $arg2
 *   This could be anything, but it probably contains the gift certificate code. :)
 */
function uc_gift_certificate_checkout_enter_code($op, &$order, $arg2) {
  global $user;
  $certificate_code = check_plain($arg2['certificate_code']);
  if ($user->mail) {
    $user_email = $user->mail;
  }
  elseif ($order->primary_email && $order->primary_email != NULL) {
    $user_email = $order->primary_email;
  }
  else {
    $user_email = NULL;
  }

  switch ($op) {
    case 'view':
      $description = variable_get('uc_gift_certificate_pane_description', 'Enter a gift certificate code for this order.');
      $contents['certificate_code'] = array(
        '#type' => 'textfield',
        '#title' => t('Certificate Code'),
        '#default_value' => $order->uc_gift_certificate_used['cert_code'],
        '#size' => 25,
      );
      return array('description' => $description, 'contents' => $contents);

    case 'process':
      // User entered a code into the form textfield. Validate it, ignoring any certificates already
      // in the user's account.
      if ($certificate_code && $certificate = uc_gift_certificate_validate($certificate_code, $user_email)) {

        if ($certificate->valid == TRUE && $order->uid > 0) {
          drupal_set_message("Attached gift certificate !code to your account.", array('!code', $certificate->cert_code));

          // Update this certificate so it now belongs to the user.
          // @todo: We may want to skip this, in case we need to ignore user's existing certificates.
          db_query('UPDATE {uc_gift_certificates} SET user_id = %d, order_id = %d WHERE certificate_id = %d',
            $order->uid, $order->order_id, $certificate->certificate_id);

          // @todo: Make sure we're attaching this certificate to the order and updating in certificate_orders table
          $certificate = uc_gift_certificate_load($certificate->certificate_id);

          // @todo: Do we need to re-attach the uc_gift_certificate_used array here?
          $order = uc_gift_certificate_attach($order, $certificate); // @todo: Combine the db_query above with this func

          // TODO: Allow users to remove the certificate? Either a button/link, or by deleting the code.
        }
        else {
          drupal_set_message($certificate->error, 'error');
          return FALSE;
        }
      }
      else {
        // If no certificate was entered, see if we can use pre-existing certificates in this order.
        $certificates = uc_gift_certificate_total($order->uid);
        if ($certificates > 0) {

          // Get the user's current certificates, and any already attached to this order.
          $order->uc_gift_certificate_used = get_uc_gift_certificates($order);
        }
      }
      break;
  }
}
